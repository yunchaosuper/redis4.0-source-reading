/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
/*
 * 创建一个链表
 *
 * 返回值
 *      链表结构体指针
 */
list *listCreate(void)
{
    struct list *list;

    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* Remove all the elements from the list without destroying the list itself. */
/*
 * 清空列表中的所有元素，不会清除链表本身
 *
 * 参数列表
 *      1.list: 待清空的链表
 */
void listEmpty(list *list)
{
    unsigned long len;
    listNode *current, *next;

    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        if (list->free) list->free(current->value);
        zfree(current);
        current = next;
    }
    list->head = list->tail = NULL;
    list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
/*
 * 释放链表以及链表中元素所占的内存
 *
 * 参数列表
 *      1.list: 待释放的链表
 *
 */
void listRelease(list *list)
{
    listEmpty(list);
    zfree(list);
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/*
 * 添加一个元素到列表头
 *
 * 参数列表
 *      1. list: 链表
 *      2. value: 待添加的值
 *
 * 返回值
 *      链表入参本身，方便级联处理
 *
 */
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len++;
    return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/*
 * 添加一个元素到列表尾部
 *
 * 参数列表
 *      1. list: 链表
 *      2. value: 待添加的值
 *
 * 返回值
 *      链表入参本身，方便级联处理
 *
 */
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    list->len++;
    return list;
}

/*
 * 插入一个节点到指定节点的前面或者后面
 *
 * 参数列表
 *      1. list: 待操作的链表
 *      2. old_node: 要插入到哪一个节点的前面或者后面
 *      3. value: 要插入的值
 *      4. after: 如果为0则插入到old_value的前面，非0则插入到old_value的后面
 *
 * 返回值
 *      返回值即是链表本身，仅是为了方便链式操作
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    // 如果不能插入新节点则返回空告诉上层调用者插入失败
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (after) {
        // 插入到指定节点的后面
        node->prev = old_node;
        node->next = old_node->next;
        // 如果正好插入到了链表的尾部则将新插入的节点设置链表尾
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        // 插入到指定节点的前面
        node->next = old_node;
        node->prev = old_node->prev;
        // 如果正好插入到了链表头部则将新的节点设置为链表头
        if (list->head == old_node) {
            list->head = node;
        }
    }
    // 设置前后节点对应的前后指针值
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    if (node->next != NULL) {
        node->next->prev = node;
    }
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
/*
 * 从链表中删除指定节点
 *
 * 参数列表
 *      1. list: 待操作的链表
 *      2. node: 待删除的节点
 */
void listDelNode(list *list, listNode *node)
{
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    if (list->free) list->free(node->value);
    zfree(node);
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
/*
 * 获取一个链表的迭代器
 *
 * 参数列表
 *      1. list: 待操作的链表
 *      2. direction: 标记从表头还是遍历还是表尾遍历,0从头开始，1从尾开始
 *
 * 返回值
 *      该链表的迭代器，是listNext()函数的入参
 */
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;

    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
/*
 * 释放链表迭代器所占的内存
 *
 * 参数列表
 *      1. iter: 待释放的迭代器
 */
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
/*
 * 重置迭代器到链表头
 *
 * 参数列表
 *      1. list: 待处理的链表
 *      2. li: 待重置的迭代器
 */
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

/*
 * 重置迭代器到链表尾部
 *
 * 参数列表
 *      1. list: 待处理的链表
 *      2. li: 待重置的迭代器
 */
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
/*
 * 获取迭代器下一个节点
 *
 * 参数列表
 *      1. iter: 链表的迭代器
 *
 * 返回值
 *      链表的下一个节点或者空(代表链表遍历完毕)
 */
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
/*
 * 深拷贝整个链表(如果没有设置单个节点的拷贝函数则为浅拷贝)
 *
 * 参数列表
 *      1. orig: 待复制的链表
 *
 * 返回值
 *      新拷贝出来的链表
 */
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    if ((copy = listCreate()) == NULL)
        return NULL;
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    listRewind(orig, &iter);
    while((node = listNext(&iter)) != NULL) {
        void *value;

        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                return NULL;
            }
        } else
            value = node->value;
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
/*
 * 搜索指定与key值匹配的节点, 返回第一个匹配的节点
 * 如果有链表中有匹配函数则使用匹配函数，否则直接判断key值地址与节点值地址是否相等
 *
 * 参数列表
 *      1. list: 待搜索的链表
 *      2. key: 用于搜索的key值
 *
 * 返回值
 *      与key值匹配的节点或者空
 */
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    // 先重置迭代器的迭代起始位置为链表头
    listRewind(list, &iter);
    // 调用链表的迭代器逐一遍历链表元素
    while((node = listNext(&iter)) != NULL) {
        // 如果链表设置了节点匹配函数则使用否则直接比较内存地址
        if (list->match) {
            if (list->match(node->value, key)) {
                return node;
            }
        } else {
            if (key == node->value) {
                return node;
            }
        }
    }
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
/*
 * 根据索引index获取指定位置的节点
 *
 * 参数列表
 *      1. list: 待查询的链表
 *      2. index: 索引值（类似数组下标），小于0则代表从后(尾部)往前索引
 *
 * 返回值
 *      指定索引的节点，如果索引超出范围则返回空
 */
listNode *listIndex(list *list, long index) {
    listNode *n;

    if (index < 0) {
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
/*
 * 将链表尾部的节点移动到链表头部
 *
 * 参数列表
 *      1. list: 待处理的链表
 *
 */
void listRotate(list *list) {
    listNode *tail = list->tail;

    if (listLength(list) <= 1) return;

    /* Detach current tail */
    list->tail = tail->prev;
    list->tail->next = NULL;
    /* Move it as head */
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
/*
 * 将列表o的所有节点追加到链表l的尾部
 *
 * 参数列表
 *      1. l: 被追加的链表
 *      2. o: 待追截的链表，其中的元素将被追加到l中，并且该链表将不再保留原节点的引用
 *
 */
void listJoin(list *l, list *o) {
    if (o->head)
        o->head->prev = l->tail;

    if (l->tail)
        l->tail->next = o->head;
    else
        l->head = o->head;

    if (o->tail) l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    o->head = o->tail = NULL;
    o->len = 0;
}

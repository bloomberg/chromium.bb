// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LINKED_LIST_H_
#define BASE_LINKED_LIST_H_

// Simple LinkedList type.
//
// To use, start by declaring the class which will be contained in the linked
// list, as extending LinkNode (this gives it next/previous pointers).
//
//   class MyNodeType : public LinkNode<MyNodeType> {
//     ...
//   };
//
// Next, to keep track of the list's head/tail, use a LinkedList instance:
//
//   LinkedList<MyNodeType> list;
//
// To add elements to the list, use any of LinkedList::Append,
// LinkNode::InsertBefore, or LinkNode::InsertAfter:
//
//   LinkNode<MyNodeType>* n1 = ...;
//   LinkNode<MyNodeType>* n2 = ...;
//   LinkNode<MyNodeType>* n3 = ...;
//
//   list.Append(n1);
//   list.Append(n3);
//   n3->InsertBefore(n3);
//
// Lastly, to iterate through the linked list forwards:
//
//   for (LinkNode<MyNodeType>* node = list.head();
//        node != list.end();
//        node = node->next()) {
//     MyNodeType* value = node->value();
//     ...
//   }
//
// Or to iterate the linked list backwards:
//
//   for (LinkNode<MyNodeType>* node = list.tail();
//        node != list.end();
//        node = node->previous()) {
//     MyNodeType* value = node->value();
//     ...
//   }
//

namespace base {

template <typename T>
class LinkNode {
 public:
  LinkNode() : previous_(0), next_(0) {}
  LinkNode(LinkNode<T>* previous, LinkNode<T>* next)
      : previous_(previous), next_(next) {}

  // Insert |this| into the linked list, before |e|.
  void InsertBefore(LinkNode<T>* e) {
    this->next_ = e;
    this->previous_ = e->previous_;
    e->previous_->next_ = this;
    e->previous_ = this;
  }

  // Insert |this| into the linked list, after |e|.
  void InsertAfter(LinkNode<T>* e) {
    this->next_ = e->next_;
    this->previous_ = e;
    e->next_->previous_ = this;
    e->next_ = this;
  }

  // Remove |this| from the linked list.
  void RemoveFromList() {
    this->previous_->next_ = this->next_;
    this->next_->previous_ = this->previous_;
  }

  LinkNode<T>* previous() const {
    return previous_;
  }

  LinkNode<T>* next() const {
    return next_;
  }

  // Cast from the node-type to the value type.
  const T* value() const {
    return reinterpret_cast<const T*>(this);
  }

  T* value() {
    return reinterpret_cast<T*>(this);
  }

 private:
  LinkNode<T>* previous_;
  LinkNode<T>* next_;
};

template <typename T>
class LinkedList {
 public:
  // The "root" node is self-referential, and forms the basis of a circular
  // list (root_.next() will point back to the start of the list,
  // and root_->previous() wraps around to the end of the list).
  LinkedList() : root_(&root_, &root_) {}

  // Appends |e| to the end of the linked list.
  void Append(LinkNode<T>* e) {
    e->InsertBefore(&root_);
  }

  LinkNode<T>* head() const {
    return root_.next();
  }

  LinkNode<T>* tail() const {
    return root_.previous();
  }

  const LinkNode<T>* end() const {
    return &root_;
  }

 private:
  LinkNode<T> root_;
};

}  // namespace base

#endif  // BASE_LINKED_LIST_H_

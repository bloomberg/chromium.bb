// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_LIST_H__
#define GESTURES_LIST_H__

#include "gestures/include/logging.h"

namespace gestures {

// Elt must have the following members:
// Elt* next_;
// Elt* prev_;

template<typename Elt>
class List {
 public:
  List() : size_(0) { sentinel_.next_ = sentinel_.prev_ = &sentinel_; }
  ~List() { DeleteAll(); }

  // inserts new_elt before existing. Assumes existing is in list already.
  void InsertBefore(Elt *existing, Elt* new_elt) {
    size_++;
    Elt* pre_new = existing->prev_;
    pre_new->next_ = new_elt;
    new_elt->prev_ = pre_new;
    new_elt->next_ = existing;
    existing->prev_ = new_elt;
  }

  Elt* Unlink(Elt* existing) {
    if (Empty()) {
      Err("Can't pop from empty list!");
      return NULL;
    }
    size_--;
    existing->prev_->next_ = existing->next_;
    existing->next_->prev_ = existing->prev_;
    existing->prev_ = existing->next_ = NULL;
    return existing;
  }

  void PushFront(Elt* elt) { InsertBefore(sentinel_.next_, elt); }
  Elt* PopFront() { return Unlink(sentinel_.next_); }
  void PushBack(Elt* elt) { InsertBefore(&sentinel_, elt); }
  Elt* PopBack() { return Unlink(sentinel_.prev_); }

  void DeleteAll() {
    while (!Empty())
      delete PopFront();
  }

  Elt* Head() const { return sentinel_.next_; }
  Elt* Tail() const { return sentinel_.prev_; }
  size_t size() const { return size_; }
  bool Empty() const { return size() == 0; }

  // Iterator-like methods
  Elt* Begin() const { return Head(); }
  Elt* End() const { return const_cast<Elt*>(&sentinel_); }

 private:
  // sentinel element
  Elt sentinel_;

  size_t size_;
};

}  // namespace gestures

#endif  // GESTURES_LIST_H__

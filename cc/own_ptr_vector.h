// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OWN_PTR_VECTOR_H_
#define CC_OWN_PTR_VECTOR_H_

#include "base/basictypes.h"
#include "base/stl_util.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/OwnPtr.h>

namespace cc {

// This type acts like a Vector<OwnPtr> but based on top of std::vector. The
// OwnPtrVector has ownership of all elements in the vector.
template <typename T>
class OwnPtrVector {
 public:
  typedef typename std::vector<T*>::iterator iterator;
  typedef typename std::vector<T*>::const_iterator const_iterator;
  typedef typename std::vector<T*>::reverse_iterator reverse_iterator;
  typedef typename std::vector<T*>::const_reverse_iterator
      const_reverse_iterator;

  OwnPtrVector() {}

  ~OwnPtrVector() { clear(); }

  size_t size() const {
    return data_.size();
  }

  T* Peek(size_t index) const {
    ASSERT(index < size());
    return data_[index];
  }

  T* operator[](size_t index) const {
    return Peek(index);
  }

  T* first() const {
    ASSERT(!isEmpty());
    return Peek(0);
  }

  T* last() const {
    ASSERT(!isEmpty());
    return Peek(size() - 1);
  }

  bool isEmpty() const {
    return size() == 0;
  }

  PassOwnPtr<T> take(size_t index) {
    ASSERT(index < size());
    OwnPtr<T> ret = adoptPtr(data_[index]);
    data_[index] = NULL;
    return ret.release();
  }

  void remove(size_t index) {
    ASSERT(index < size());
    delete data_[index];
    data_.erase(data_.begin() + index);
  }

  void clear() {
    STLDeleteElements(&data_);
  }

  void append(PassOwnPtr<T> item) {
    data_.push_back(item.leakPtr());
  }

  void insert(size_t index, PassOwnPtr<T> item) {
    ASSERT(index < size());
    data_.insert(data_.begin() + index, item.leakPtr());
  }

  iterator begin() { return data_.begin(); }
  const_iterator begin() const { return data_.begin(); }
  iterator end() { return data_.end(); }
  const_iterator end() const { return data_.end(); }

  reverse_iterator rbegin() { return data_.rbegin(); }
  const_reverse_iterator rbegin() const { return data_.rbegin(); }
  reverse_iterator rend() { return data_.rend(); }
  const_reverse_iterator rend() const { return data_.rend(); }

 private:
  std::vector<T*> data_;

  DISALLOW_COPY_AND_ASSIGN(OwnPtrVector);
};

}  // namespace cc

#endif  // CC_OWN_PTR_VECTOR_H_

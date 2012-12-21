// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCOPED_PTR_VECTOR_H_
#define CC_SCOPED_PTR_VECTOR_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"

namespace cc {

// This type acts like a vector<scoped_ptr> based on top of std::vector. The
// ScopedPtrVector has ownership of all elements in the vector.
template <typename T>
class ScopedPtrVector {
 public:
  typedef typename std::vector<T*>::iterator iterator;
  typedef typename std::vector<T*>::const_iterator const_iterator;
  typedef typename std::vector<T*>::reverse_iterator reverse_iterator;
  typedef typename std::vector<T*>::const_reverse_iterator
      const_reverse_iterator;

  ScopedPtrVector() {}

  ~ScopedPtrVector() { clear(); }

  size_t size() const {
    return data_.size();
  }

  T* Peek(size_t index) const {
    DCHECK(index < size());
    return data_[index];
  }

  T* operator[](size_t index) const {
    return Peek(index);
  }

  T* first() const {
    DCHECK(!isEmpty());
    return Peek(0);
  }

  T* last() const {
    DCHECK(!isEmpty());
    return Peek(size() - 1);
  }

  bool isEmpty() const {
    return size() == 0;
  }

  scoped_ptr<T> take(size_t index) {
    DCHECK(index < size());
    scoped_ptr<T> ret(data_[index]);
    data_[index] = NULL;
    return ret.Pass();
  }

  void remove(size_t index) {
    DCHECK(index < size());
    delete data_[index];
    data_.erase(data_.begin() + index);
  }

  void reserve(size_t size) {
    data_.reserve(size);
  }

  void clear() {
    STLDeleteElements(&data_);
  }

  void append(scoped_ptr<T> item) {
    data_.push_back(item.release());
  }

  void insert(size_t index, scoped_ptr<T> item) {
    DCHECK(index <= size());
    data_.insert(data_.begin() + index, item.release());
  }

  void swap(ScopedPtrVector<T>& other) {
    data_.swap(other.data_);
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

  DISALLOW_COPY_AND_ASSIGN(ScopedPtrVector);
};

}  // namespace cc

#endif  // CC_SCOPED_PTR_VECTOR_H_

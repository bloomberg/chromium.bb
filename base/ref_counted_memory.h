// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_REF_COUNTED_MEMORY_H_
#define BASE_REF_COUNTED_MEMORY_H_

#include <vector>

#include "base/ref_counted.h"

// TODO(erg): The contents of this file should be in a namespace. This would
// require touching >100 files in chrome/ though.

// A generic interface to memory. This object is reference counted because one
// of its two subclasses own the data they carry, and we need to have
// heterogeneous containers of these two types of memory.
class RefCountedMemory : public base::RefCountedThreadSafe< RefCountedMemory > {
 public:
  virtual ~RefCountedMemory() {}

  // Retrieves a pointer to the beginning of the data we point to.
  virtual const unsigned char* front() const = 0;

  // Size of the memory pointed to.
  virtual size_t size() const = 0;
};

// An implementation of RefCountedMemory, where the ref counting does not
// matter.
class RefCountedStaticMemory : public RefCountedMemory {
 public:
  RefCountedStaticMemory()
      : data_(NULL), length_(0) {}
  RefCountedStaticMemory(const unsigned char* data, size_t length)
      : data_(data), length_(length) {}

  virtual const unsigned char* front() const { return data_; }
  virtual size_t size() const { return length_; }

 private:
  const unsigned char* data_;
  size_t length_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedStaticMemory);
};

// An implementation of RefCountedMemory, where we own our the data in a
// vector.
class RefCountedBytes : public RefCountedMemory {
 public:
  // Constructs a RefCountedBytes object by performing a swap. (To non
  // destructively build a RefCountedBytes, use the constructor that takes a
  // vector.)
  static RefCountedBytes* TakeVector(std::vector<unsigned char>* to_destroy) {
    RefCountedBytes* bytes = new RefCountedBytes;
    bytes->data.swap(*to_destroy);
    return bytes;
  }

  RefCountedBytes() {}

  // Constructs a RefCountedBytes object by _copying_ from |initializer|.
  RefCountedBytes(const std::vector<unsigned char>& initializer)
      : data(initializer) {}

  virtual const unsigned char* front() const { return &data.front(); }
  virtual size_t size() const { return data.size(); }

  std::vector<unsigned char> data;

 private:
  DISALLOW_COPY_AND_ASSIGN(RefCountedBytes);
};

#endif  // BASE_REF_COUNTED_MEMORY_H_

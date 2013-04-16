// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_MEMORY_MANAGER_H_
#define GESTURES_MEMORY_MANAGER_H_

#include <new>

#include "gestures/include/logging.h"

namespace gestures {

// A simple memory manager class that pre-allocates a size of
// buffer and garbage collects freed variables. It is not intended
// to be used directly. User classes should inherit the
// MemoryManaged wrapper class and use the provided Allocate/Free
// interface instead (see below).

template<typename T>
class MemoryManager {
 public:
  explicit MemoryManager(size_t size) : buf_(new T[size]),
    free_slots_(new T *[size]), used_mark_(new bool[size]()),
    max_size_(size), head_(size) {
    for (size_t i = 0; i < max_size_; i++)
      free_slots_[i] = buf_.get() + i;
  }

  size_t Size() const { return max_size_ - head_; }
  size_t MaxSize() const { return max_size_; }

  T* Allocate() {
    if (!head_) {
      Err("MemoryManager::Allocate: out of space");
      return NULL;
    }

    T* ptr = free_slots_[--head_];
    used_mark_[ptr - buf_.get()] = true;
    return ptr;
  }

  bool Free(T* ptr) {
    // Check for invalid pointer and double-free
    if (ptr < buf_.get() || ptr >= buf_.get() + max_size_) {
      Err("MemoryManager::Free: pointer out of bounds");
      return false;
    }
    size_t offset_in_bytes = reinterpret_cast<size_t>(ptr) -
        reinterpret_cast<size_t>(buf_.get());
    if (offset_in_bytes % sizeof(T)) {
      Err("MemoryManager::Free: unaligned pointer");
      return false;
    }
    size_t offset = ptr - buf_.get();
    if (!used_mark_[offset]) {
      Err("MemoryManager::Free: double-free");
      return false;
    }

    free_slots_[head_++] = ptr;
    used_mark_[offset] = false;
    return true;
  }

 private:
  scoped_array<T> buf_;
  scoped_array<T*> free_slots_;
  scoped_array<bool> used_mark_;
  size_t max_size_;
  size_t head_;
  DISALLOW_COPY_AND_ASSIGN(MemoryManager<T>);
};

// A simple wrapper of the MemoryManager. To use it, public
// inherit the MemoryManaged class and initialize the internal
// allocation size somewhere before we enter the interpretation
// path (e.g. in the filter class's ctor):
//
// struct MyClass : public MemoryManaged<MyClass> {
// };
//
// class MyFilterInterpreter {
//  public:
//   MyFilterInterpreter(...) {
//     MyClass::InitMemoryManager(size_to_use);
//   }
// }
//
// You can also extend existing classes (e.g. map/set/vector/list)
// like this:
//
// template<typename T>
// class ManagedList: public List<T, MemoryManagedDeallocator<T>>,
//                    public MemoryManaged<ManagedList<T>> {
// };
//
// After the setup, you may safely use the allocate/free interfaces to
// dynamically allocate/free instances of your class in the SyncInterpret
// path without worrying the deadlock problem. MemoryManager would handle
// these operations with the pre-allocated memory block. Please note that
// we specially choose not to overload the new/delete operators because
// using them around is very dangerous which may accidentally call
// malloc/free and hang the UI thread.
//
// An example of usage:
//
// MyClass* ptr = MyClass::Allocate();
// if (!ptr)
//   return;
//
// // Initialized the memory chunk with placement new
// new (ptr) MyClass(ctor_arg1, ctor_arg2);
//
// // Free the variable
// MyClass::Free(ptr);

template<typename T>
class MemoryManaged {
 public:
  MemoryManaged() {}

  static T* Allocate() {
    if (!memory_manager_.get()) {
      Err("MemoryManaged::new: no manager available");
      return NULL;
    }
    return memory_manager_->Allocate();
  }

  static bool Free(T* ptr) {
    if (!memory_manager_.get()) {
      Err("MemoryManaged::delete: no manager available");
      return false;
    }
    (*ptr).~T();
    return memory_manager_->Free(ptr);
  }

  static bool HasMemoryAvailable() {
    return memory_manager_->Size() < memory_manager_->MaxSize();
  }

  // Need to be called before we enter the interpretation path
  // and before any new/delete takes place
  static void InitMemoryManager(size_t size) {
    memory_manager_.reset(new MemoryManager<T> (size));
  }

 private:
  // We use scoped_ptr so it is auto-cleared upon the program exit
  static scoped_ptr<MemoryManager<T>> memory_manager_;
};

template<typename T>
scoped_ptr<MemoryManager<T>> MemoryManaged<T>::memory_manager_;

// A deallocator class for the memory managed types. It could be used in the
// List template class. Class T must inherit the MemoryManaged class.

template<typename T>
class MemoryManagedDeallocator {
 public:
  bool Deallocate(T* elt) {
    return T::Free(elt);
  }
};

}  // namespace gestures

#endif  // MEMORY_MANAGER_H_

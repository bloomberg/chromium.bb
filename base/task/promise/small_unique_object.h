// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_SMALL_UNIQUE_OBJECT_H_
#define BASE_TASK_PROMISE_SMALL_UNIQUE_OBJECT_H_

#include <cstring>

#include "base/macros.h"
#include "base/template_util.h"

namespace base {
namespace internal {

// A container intended for inline storage of a derived virtual object up to a
// maximum size. This is useful to avoid unnecessary heap allocations.
template <typename T, size_t MaxSize = sizeof(int*) * 3>
class BASE_EXPORT SmallUniqueObject {
 public:
  SmallUniqueObject() { memset(&union_, 0, sizeof(union_)); }

  template <typename Derived, typename... Args>
  SmallUniqueObject(in_place_type_t<Derived>, Args&&... args) noexcept {
    static_assert(std::is_base_of<T, Derived>::value,
                  "T is not a base of Derived");
    static_assert(sizeof(Derived) <= MaxSize,
                  "Derived is too big to be held by SmallUniqueObject");
    static_assert(sizeof(T) <= MaxSize,
                  "T is too big to be held by SmallUniqueObject");
    new (union_.storage) Derived(std::forward<Args>(args)...);
  }

  ~SmallUniqueObject() { reset(); }

  explicit operator bool() const { return !Empty(); }

  bool Empty() const {
    for (size_t i = 0; i < kMaxInts; i++) {
      if (union_.flag[i])
        return false;
    }
    return true;
  }

  void reset() {
    if (!Empty()) {
      get()->~T();
      memset(&union_, 0, sizeof(union_));
    }
  }

  T* get() noexcept {
    if (Empty())
      return nullptr;
    return reinterpret_cast<T*>(union_.storage);
  }

  const T* get() const noexcept {
    if (Empty())
      return nullptr;
    return reinterpret_cast<const T*>(union_.storage);
  }

  const T& operator*() const {
    DCHECK(!Empty());
    return *get();
  }

  T& operator*() {
    DCHECK(!Empty());
    return *get();
  }

  const T* operator->() const noexcept {
    DCHECK(!Empty());
    return get();
  }

  T* operator->() noexcept {
    DCHECK(!Empty());
    return get();
  }

 private:
  static constexpr size_t kMaxInts = MaxSize / sizeof(int);
  union {
    // The vtable will be stored somewhere within |storage|, the actual location
    // is compiler specific but where ever it is, it can't be zero.
    int flag[kMaxInts];
    char storage[MaxSize];
  } union_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_SMALL_UNIQUE_OBJECT_H_

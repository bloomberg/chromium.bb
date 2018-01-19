// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NO_DESTRUCTOR_H_
#define BASE_NO_DESTRUCTOR_H_

#include <utility>

namespace base {

// A wrapper that makes it easy to create an object of type T with static
// storage duration that:
// - is only constructed on first access
// - never invokes the destructor
// in order to satisfy the styleguide ban on global constructors and
// destructors.
//
// Runtime constant example:
// const std::string& GetLineSeparator() {
//  // Forwards to std::string(size_t, char, const Allocator&) constructor.
//   static const base::NoDestructor<std::string> s(5, '-');
//   return s;
// }
//
// More complex initialization with a lambda:
// const std::string& GetSessionNonce() {
//   static const base::NoDestructor<std::string> nonce([] {
//     std::string s(16);
//     crypto::RandString(s.data(), s.size());
//     return s;
//   })());
//   return *nonce;
// }
//
// NoDestructor<T> stores the object inline, so it also avoids a pointer
// indirection and a malloc. Code should prefer to use NoDestructor<T> over:
// - The CR_DEFINE_STATIC_LOCAL() helper macro.
// - A function scoped static T* or T& that is dynamically initialized.
// - A global base::LazyInstance<T>.
//
// Note that since the destructor is never run, this *will* leak memory if used
// as a stack or member variable. Furthermore, a NoDestructor<T> should never
// have global scope as that may require a static initializer.
template <typename T>
class NoDestructor {
 public:
  // Not constexpr; just write static constexpr T x = ...; if the value should
  // be a constexpr.
  template <typename... Args>
  explicit NoDestructor(Args&&... args) {
    new (get()) T(std::forward<Args>(args)...);
  }

  ~NoDestructor() = default;

  const T& operator*() const { return *get(); }
  T& operator*() { return *get(); }

  const T* operator->() const { return get(); }
  T* operator->() { return get(); }

  const T* get() const { return reinterpret_cast<const T*>(&storage_); }
  T* get() { return reinterpret_cast<T*>(&storage_); }

 private:
  alignas(T) char storage_[sizeof(T)];
};

}  // namespace base

#endif  // BASE_NO_DESTRUCTOR_H_

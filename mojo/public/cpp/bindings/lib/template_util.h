// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_TEMPLATE_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_TEMPLATE_UTIL_H_

#include <type_traits>

namespace mojo {
namespace internal {

template <class T, T v>
struct IntegralConstant {
  static const T value = v;
};

template <class T, T v>
const T IntegralConstant<T, v>::value;

typedef IntegralConstant<bool, true> TrueType;
typedef IntegralConstant<bool, false> FalseType;

template <class T>
struct IsConst : FalseType {};
template <class T>
struct IsConst<const T> : TrueType {};

template <class T>
struct IsPointer : FalseType {};
template <class T>
struct IsPointer<T*> : TrueType {};

template <bool B, typename T = void>
struct EnableIf {};

template <typename T>
struct EnableIf<true, T> {
  typedef T type;
};

// Types YesType and NoType are guaranteed such that sizeof(YesType) <
// sizeof(NoType).
typedef char YesType;

struct NoType {
  YesType dummy[2];
};

// A helper template to determine if given type is non-const move-only-type,
// i.e. if a value of the given type should be passed via std::move() in a
// destructive way.
template <typename T>
struct IsMoveOnlyType {
  static const bool value = std::is_constructible<T, T&&>::value &&
                            !std::is_constructible<T, const T&>::value;
};

// This goop is a trick used to implement a template that can be used to
// determine if a given class is the base class of another given class.
template <typename, typename>
struct IsSame {
  static bool const value = false;
};
template <typename A>
struct IsSame<A, A> {
  static bool const value = true;
};

template <typename T>
struct EnsureTypeIsComplete {
  // sizeof() cannot be applied to incomplete types, this line will fail
  // compilation if T is forward declaration.
  using CheckSize = char (*)[sizeof(T)];
};

template <typename Base, typename Derived>
struct IsBaseOf {
 private:
  static Derived* CreateDerived();
  static char(&Check(Base*))[1];
  static char(&Check(...))[2];

  EnsureTypeIsComplete<Base> check_base_;
  EnsureTypeIsComplete<Derived> check_derived_;

 public:
  static bool const value = sizeof Check(CreateDerived()) == 1 &&
                            !IsSame<Base const, void const>::value;
};

template <class T>
struct RemovePointer {
  typedef T type;
};
template <class T>
struct RemovePointer<T*> {
  typedef T type;
};

template <template <typename...> class Template, typename T>
struct IsSpecializationOf : FalseType {};

template <template <typename...> class Template, typename... Args>
struct IsSpecializationOf<Template, Template<Args...>> : TrueType {};

template <bool B, typename T, typename F>
struct Conditional {
  typedef T type;
};

template <typename T, typename F>
struct Conditional<false, T, F> {
  typedef F type;
};

template <typename T>
struct AlwaysFalse {
  static const bool value = false;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_TEMPLATE_UTIL_H_

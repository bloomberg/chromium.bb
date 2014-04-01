// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_CALLBACK_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_CALLBACK_INTERNAL_H_

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

namespace mojo {
namespace internal {

template <typename T, bool is_object_type = TypeTraits<T>::kIsObject>
struct Callback_ParamTraits {};

template <typename T>
struct Callback_ParamTraits<T, true> {
  typedef const T& ForwardType;
  static const bool kIsScopedHandle = false;
};

template <typename T>
struct Callback_ParamTraits<T, false> {
  typedef T ForwardType;
  static const bool kIsScopedHandle = false;
};

template <typename H>
struct Callback_ParamTraits<ScopedHandleBase<H>, true> {
  typedef ScopedHandleBase<H> ForwardType;
  static const bool kIsScopedHandle = true;
};

template<bool B, typename T = void>
struct EnableIf {};

template<typename T>
struct EnableIf<true, T> { typedef T type; };

template <typename T>
typename EnableIf<!Callback_ParamTraits<T>::kIsScopedHandle, T>::type&
    Callback_Forward(T& t) {
  return t;
}

template <typename T>
typename EnableIf<Callback_ParamTraits<T>::kIsScopedHandle, T>::type
    Callback_Forward(T& t) {
  return t.Pass();
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_CALLBACK_INTERNAL_H_

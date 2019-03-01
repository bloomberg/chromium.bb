// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CALL_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CALL_INTERNAL_H_

#include <utility>

#include "base/component_export.h"
#include "base/macros.h"

namespace mojo {
namespace internal {

// Base class for temporary objects returned by |Remote<T>::rpc()|. This
// provides pass-through access to the passed calling proxy.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) CallProxyWrapperBase {
 public:
  explicit CallProxyWrapperBase(void* proxy);
  CallProxyWrapperBase(CallProxyWrapperBase&& other);
  ~CallProxyWrapperBase();

  void* proxy() const { return proxy_; }

 private:
  void* proxy_;

  DISALLOW_COPY_AND_ASSIGN(CallProxyWrapperBase);
};

template <typename T>
class CallProxyWrapper : public CallProxyWrapperBase {
 public:
  explicit CallProxyWrapper(T* proxy) : CallProxyWrapperBase(proxy) {}
  CallProxyWrapper(CallProxyWrapper&& other)
      : CallProxyWrapperBase(std::move(other)) {}

  T* operator->() const { return static_cast<T*>(proxy()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(CallProxyWrapper);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CALL_INTERNAL_H_

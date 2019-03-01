// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/call_internal.h"

namespace mojo {
namespace internal {

CallProxyWrapperBase::CallProxyWrapperBase(void* proxy) : proxy_(proxy) {}

CallProxyWrapperBase::CallProxyWrapperBase(CallProxyWrapperBase&& other)
    : proxy_(other.proxy_) {}

CallProxyWrapperBase::~CallProxyWrapperBase() = default;

}  // namespace internal
}  // namespace mojo

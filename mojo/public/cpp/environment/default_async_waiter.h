// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_ENVIRONMENT_DEFAULT_ASYNC_WAITER_H_
#define MOJO_PUBLIC_CPP_ENVIRONMENT_DEFAULT_ASYNC_WAITER_H_

#include "mojo/public/c/environment/async_waiter.h"

namespace mojo {

// Returns a default implementation of MojoAsyncWaiter.
MojoAsyncWaiter* GetDefaultAsyncWaiter();

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_ENVIRONMENT_DEFAULT_ASYNC_WAITER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/environment/default_async_waiter.h"

#include "mojo/environment/default_async_waiter_impl.h"

namespace mojo {

MojoAsyncWaiter* GetDefaultAsyncWaiter() {
  return internal::GetDefaultAsyncWaiterImpl();
}

}  // namespace mojo

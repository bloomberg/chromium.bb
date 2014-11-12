// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/environment/default_run_loop_impl.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace mojo {
namespace internal {

void InstantiateDefaultRunLoopImpl() {
  // Not leaked: accessible from |base::MessageLoop::current()|.
  new base::MessageLoop();
}

void DestroyDefaultRunLoopImpl() {
  delete base::MessageLoop::current();
}

}  // namespace internal
}  // namespace mojo

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application_impl.h"

namespace mojo {

void ApplicationImpl::Terminate() {
  if (base::MessageLoop::current()->is_running())
    base::MessageLoop::current()->Quit();
}

}  // namespace mojo

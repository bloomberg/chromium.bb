// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/utility/run_loop.h"

namespace mojo {

void ApplicationImpl::Terminate() {
  mojo::RunLoop::current()->Quit();
}

}  // namespace mojo

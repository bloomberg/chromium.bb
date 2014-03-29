// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/environment/environment.h"

#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/environment/lib/buffer_tls_setup.h"

namespace mojo {

Environment::Environment() {
  internal::SetUpCurrentBuffer();
  RunLoop::SetUp();
}

Environment::~Environment() {
  RunLoop::TearDown();
  internal::TearDownCurrentBuffer();
}

}  // namespace mojo

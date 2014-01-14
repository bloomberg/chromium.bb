// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/environment/environment.h"

#include "mojo/public/environment/standalone/buffer_tls_setup.h"
#include "mojo/public/utility/run_loop.h"

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

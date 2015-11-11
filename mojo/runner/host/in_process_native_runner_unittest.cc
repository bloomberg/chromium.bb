// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/host/in_process_native_runner.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace runner {

TEST(InProcessNativeRunnerTest, NotStarted) {
  InProcessNativeRunner runner;
  // Shouldn't crash or DCHECK on destruction.
}

}  // namespace runner
}  // namespace mojo

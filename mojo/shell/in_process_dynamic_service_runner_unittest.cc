// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/context.h"
#include "mojo/shell/in_process_dynamic_service_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {

TEST(InProcessDynamicServiceRunnerTest, NotStarted) {
  Context context;
  base::MessageLoop loop;
  context.Init();
  InProcessDynamicServiceRunner runner(&context);
  // Shouldn't crash or DCHECK on destruction.
}

}  // namespace shell
}  // namespace mojo

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/in_process_native_runner.h"

#include "base/path_service.h"
#include "mojo/runner/context.h"
#include "mojo/runner/tracer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace runner {

TEST(InProcessNativeRunnerTest, NotStarted) {
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  Context context(shell_dir, nullptr);
  base::MessageLoop loop;
  context.Init();
  InProcessNativeRunner runner(&context);
  context.Shutdown();
  // Shouldn't crash or DCHECK on destruction.
}

}  // namespace runner
}  // namespace mojo

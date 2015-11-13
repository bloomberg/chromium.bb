// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/mock_sysui/mock_sysui.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new MockSysUI);
  return runner.Run(shell_handle);
}

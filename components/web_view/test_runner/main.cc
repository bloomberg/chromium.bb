// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "components/web_view/test_runner/launcher.h"
#include "mojo/runner/host/child_process.h"
#include "mojo/runner/init.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager at_exit;
  mojo::runner::InitializeLogging();
  return web_view::LaunchTestRunner(argc, argv);
}

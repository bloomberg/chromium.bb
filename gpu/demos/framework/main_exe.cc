// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/logging.h"
#include "gpu/demos/framework/window.h"

namespace {
static const int kWindowWidth = 512;
static const int kWindowHeight = 512;
}  // namespace.

int main(int argc, char *argv[]) {
  // AtExitManager is used by singleton classes to delete themselves when
  // the program terminates.
  base::AtExitManager at_exit_manager_;

  gpu::demos::Window window;
  CHECK(window.Init(kWindowWidth, kWindowHeight));

  window.MainLoop();
  return EXIT_SUCCESS;
}

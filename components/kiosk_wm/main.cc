// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/kiosk_wm/kiosk_wm.h"

#include "mojo/application/application_runner_chromium.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new kiosk_wm::KioskWM);
  return runner.Run(shell_handle);
}

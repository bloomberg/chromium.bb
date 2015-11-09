// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mojo_runner_util.h"

#include "base/command_line.h"

bool IsRunningInMojoRunner() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      "mojo-platform-channel-handle");
}

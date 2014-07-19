// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"

namespace switches {

const char kForceAuxiliaryBitmap[] = "force-auxiliary-bitmap";

bool ForceAuxiliaryBitmap() {
  static bool force_auxiliary_bitmap =
      CommandLine::ForCurrentProcess()->HasSwitch(kForceAuxiliaryBitmap);
  return force_auxiliary_bitmap;
}

}  // namespace switches

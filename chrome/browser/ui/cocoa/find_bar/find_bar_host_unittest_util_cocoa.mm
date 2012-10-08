// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"

#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"

namespace chrome {

void DisableFindBarAnimationsDuringTesting(bool /* disable */) {
  FindBarBridge::disable_animations_during_testing_ = true;
}

}  // namespace chrome

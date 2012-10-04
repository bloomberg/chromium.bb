// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"

#include "chrome/browser/ui/views/dropdown_bar_host.h"

namespace chrome {

void DisableFindBarAnimationsDuringTesting(bool disable) {
  DropdownBarHost::disable_animations_during_testing_ = disable;
}

}  // namespace chrome

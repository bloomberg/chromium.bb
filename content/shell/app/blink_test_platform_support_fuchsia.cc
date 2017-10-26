// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/blink_test_platform_support.h"

namespace content {

bool CheckLayoutSystemDeps() {
  return false;
}

bool BlinkTestPlatformInitialize() {
  // TODO(fuchsia): Support Blink's layout test platform (crbug.com/778467).
  return false;
}

}  // namespace content

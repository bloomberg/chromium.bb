// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#include "ios/public/consumer/base/util.h"

namespace ios {

bool IsRunningOnOrLater(int major, int minor, int bug_fix) {
  return base::ios::IsRunningOnOrLater(major, minor, bug_fix);
}

}  // namespace ios

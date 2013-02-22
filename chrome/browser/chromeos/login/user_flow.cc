// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_flow.h"

namespace chromeos {

UserFlow::~UserFlow() {}

DefaultUserFlow::~DefaultUserFlow() {}

bool DefaultUserFlow::ShouldLaunchBrowser() {
  return true;
}

bool DefaultUserFlow::ShouldSkipPostLoginScreens() {
  return false;
}

void DefaultUserFlow::LaunchExtraSteps() {
}

}  // namespace chromeos

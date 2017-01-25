// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

namespace tests_hook {

bool DisableContextualSearch() {
  return false;
}
bool DisableFirstRun() {
  return false;
}
bool DisableGeolocation() {
  return false;
}
bool DisableSigninRecallPromo() {
  return false;
}
bool DisableUpdateService() {
  return false;
}
void SetUpTestsIfPresent() {}
void RunTestsIfPresent() {}

}  // namespace tests_hook

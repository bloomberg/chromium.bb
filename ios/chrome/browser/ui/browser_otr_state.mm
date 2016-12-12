// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/browser_otr_state.h"

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

bool IsOffTheRecordSessionActive() {
  ios::ChromeBrowserProvider* chrome_browser_provider =
      ios::GetChromeBrowserProvider();
  return chrome_browser_provider &&
         chrome_browser_provider->IsOffTheRecordSessionActive();
}

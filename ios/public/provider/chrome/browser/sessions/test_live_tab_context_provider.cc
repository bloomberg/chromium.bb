// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/sessions/test_live_tab_context_provider.h"

sessions::LiveTabContext* TestLiveTabContextProvider::Create(
    ios::ChromeBrowserState* browser_state) {
  return nullptr;
}

sessions::LiveTabContext* TestLiveTabContextProvider::FindContextWithID(
    int32_t desired_id) {
  return nullptr;
}

sessions::LiveTabContext* TestLiveTabContextProvider::FindContextForTab(
    const sessions::LiveTab* tab) {
  return nullptr;
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TEST_LIVE_TAB_CONTEXT_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TEST_LIVE_TAB_CONTEXT_PROVIDER_H_

#include "base/macros.h"
#include "ios/public/provider/chrome/browser/sessions/live_tab_context_provider.h"

class TestLiveTabContextProvider : public ios::LiveTabContextProvider {
 public:
  TestLiveTabContextProvider() {}
  ~TestLiveTabContextProvider() override {}

  // LiveTabContextProvider.
  sessions::LiveTabContext* Create(
      ios::ChromeBrowserState* browser_state) override;
  sessions::LiveTabContext* FindContextWithID(int32_t desired_id) override;
  sessions::LiveTabContext* FindContextForTab(
      const sessions::LiveTab* tab) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLiveTabContextProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TEST_LIVE_TAB_CONTEXT_PROVIDER_H_

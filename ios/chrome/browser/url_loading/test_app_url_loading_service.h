// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_TEST_APP_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_TEST_APP_URL_LOADING_SERVICE_H_

#include "ios/chrome/browser/url_loading/app_url_loading_service.h"

@class OpenNewTabCommand;

// Service used to manage url loading at application level.
class TestAppUrlLoadingService : public AppUrlLoadingService {
 public:
  TestAppUrlLoadingService();

  // Opens a url based on |command| in a new tab.
  void LoadUrlInNewTab(OpenNewTabCommand* command) override;

  // These are the last parameters passed to |LoadUrlInNewTab|.
  OpenNewTabCommand* last_command;
  int open_new_tab_call_count = 0;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_APP_URL_LOADING_SERVICE_H_

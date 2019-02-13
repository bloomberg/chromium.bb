// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_TEST_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_TEST_URL_LOADING_SERVICE_H_

#include "ios/chrome/browser/url_loading/url_loading_service.h"

class TestUrlLoadingService : public UrlLoadingService {
 public:
  TestUrlLoadingService(UrlLoadingNotifier* notifier);

  // Opens a url based on |chrome_params|.
  void LoadUrlInCurrentTab(const ChromeLoadParams& chrome_params) override;

  // Switches to a tab that matches |web_params| or opens in a new tab.
  void SwitchToTab(
      const web::NavigationManager::WebLoadParams& web_params) override;

  // Opens a url based on |command| in a new tab.
  void OpenUrlInNewTab(OpenNewTabCommand* command) override;

  // These are the last parameters passed to |LoadUrlInCurrentTab|,
  // |OpenUrlInNewTab| and |SwitchToTab|.
  // TODO(crbug.com/907527): normalize these when all parameters in
  // UrlLoadingService are.
  web::NavigationManager::WebLoadParams last_web_params;
  WindowOpenDisposition last_disposition;
  OpenNewTabCommand* last_command;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_TEST_URL_LOADING_SERVICE_H_

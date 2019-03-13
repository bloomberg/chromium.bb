// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_TEST_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_TEST_URL_LOADING_SERVICE_H_

#include "ios/chrome/browser/url_loading/url_loading_service.h"

struct UrlLoadParams;

class TestUrlLoadingService : public UrlLoadingService {
 public:
  TestUrlLoadingService(UrlLoadingNotifier* notifier);

  // These are the last parameters passed to |OpenUrl|.
  UrlLoadParams* last_params = nullptr;
  int load_current_tab_call_count = 0;
  int switch_tab_call_count = 0;
  int load_new_tab_call_count = 0;

 private:
  // Switches to a tab that matches |params.web_params| or opens in a new tab.
  void SwitchToTab(UrlLoadParams* params) override;

  // Opens a url based on |chrome_params|.
  // TODO(crbug.com/907527): to be deprecated, use OpenUrl.
  void LoadUrlInCurrentTab(UrlLoadParams* params) override;

  // Opens a url based on |command| in a new tab.
  // TODO(crbug.com/907527): to be deprecated, use OpenUrl.
  void LoadUrlInNewTab(UrlLoadParams* params) override;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_TEST_URL_LOADING_SERVICE_H_

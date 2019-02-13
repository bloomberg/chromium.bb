// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/test_url_loading_service.h"

#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestUrlLoadingService::TestUrlLoadingService(UrlLoadingNotifier* notifier)
    : UrlLoadingService(notifier), last_web_params(GURL()) {}

void TestUrlLoadingService::LoadUrlInCurrentTab(
    const ChromeLoadParams& chrome_params) {
  last_web_params = chrome_params.web_params;
  last_disposition = chrome_params.disposition;
}

void TestUrlLoadingService::SwitchToTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  last_web_params = web_params;
  last_disposition = WindowOpenDisposition::SWITCH_TO_TAB;
}

void TestUrlLoadingService::OpenUrlInNewTab(OpenNewTabCommand* command) {
  last_command = command;
}

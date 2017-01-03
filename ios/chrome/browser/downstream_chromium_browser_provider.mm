// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/downstream_chromium_browser_provider.h"

#import <UIKit/UIKit.h>

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate_getter.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/browser_list_ios.h"
#include "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"

DownstreamChromiumBrowserProvider::DownstreamChromiumBrowserProvider() {}

DownstreamChromiumBrowserProvider::~DownstreamChromiumBrowserProvider() {}

autofill::CardUnmaskPromptView*
DownstreamChromiumBrowserProvider::CreateCardUnmaskPromptView(
    autofill::CardUnmaskPromptController* controller) {
  return new autofill::CardUnmaskPromptViewBridge(controller);
}

bool DownstreamChromiumBrowserProvider::IsOffTheRecordSessionActive() {
  return BrowserListIOS::IsOffTheRecordSessionActive();
}

void DownstreamChromiumBrowserProvider::GetFaviconForURL(
    ios::ChromeBrowserState* browser_state,
    const GURL& page_url,
    const std::vector<int>& desired_sizes_in_pixel,
    const favicon_base::FaviconResultsCallback& callback) const {
  ChromeWebUIIOSControllerFactory::GetInstance()->GetFaviconForURL(
      browser_state, page_url, desired_sizes_in_pixel, callback);
}

std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
DownstreamChromiumBrowserProvider::CreateSyncedWindowDelegatesGetter(
    ios::ChromeBrowserState* browser_state) {
  return base::MakeUnique<TabModelSyncedWindowDelegatesGetter>(browser_state);
}

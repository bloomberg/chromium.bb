// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"

TestChromeBrowserStateManager::TestChromeBrowserStateManager(
    const base::FilePath& user_data_dir)
    : browser_state_info_cache_(local_state_.Get(), user_data_dir) {}

TestChromeBrowserStateManager::~TestChromeBrowserStateManager() {}

ios::ChromeBrowserState*
TestChromeBrowserStateManager::GetLastUsedBrowserState() {
  return nullptr;
}

ios::ChromeBrowserState* TestChromeBrowserStateManager::GetBrowserState(
    const base::FilePath& path) {
  return nullptr;
}

BrowserStateInfoCache*
TestChromeBrowserStateManager::GetBrowserStateInfoCache() {
  return &browser_state_info_cache_;
}

std::vector<ios::ChromeBrowserState*>
TestChromeBrowserStateManager::GetLoadedBrowserStates() {
  return std::vector<ios::ChromeBrowserState*>();
}

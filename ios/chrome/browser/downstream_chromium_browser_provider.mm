// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/downstream_chromium_browser_provider.h"

#import <UIKit/UIKit.h>

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate_getter.h"

DownstreamChromiumBrowserProvider::DownstreamChromiumBrowserProvider() {}

DownstreamChromiumBrowserProvider::~DownstreamChromiumBrowserProvider() {}

std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
DownstreamChromiumBrowserProvider::CreateSyncedWindowDelegatesGetter(
    ios::ChromeBrowserState* browser_state) {
  return base::MakeUnique<TabModelSyncedWindowDelegatesGetter>(browser_state);
}

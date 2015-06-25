// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/browser_state/browser_state_info_cache.h"

#include "ios/public/provider/chrome/browser/browser_state/browser_state_info_cache_observer.h"

namespace ios {

BrowserStateInfoCache::BrowserStateInfoCache() {
}

BrowserStateInfoCache::~BrowserStateInfoCache() {
}

void BrowserStateInfoCache::AddObserver(
    BrowserStateInfoCacheObserver* observer) {
  observer_list_.AddObserver(observer);
}

void BrowserStateInfoCache::RemoveObserver(
    BrowserStateInfoCacheObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowserStateInfoCache::NotifyBrowserStateAdded(
    const base::FilePath& path) {
  FOR_EACH_OBSERVER(BrowserStateInfoCacheObserver, observer_list_,
                    OnBrowserStateAdded(path));
}

void BrowserStateInfoCache::NotifyBrowserStateRemoved(
    const base::FilePath& profile_path,
    const base::string16& name) {
  FOR_EACH_OBSERVER(BrowserStateInfoCacheObserver, observer_list_,
                    OnBrowserStateWasRemoved(profile_path, name));
}

}  // namespace ios

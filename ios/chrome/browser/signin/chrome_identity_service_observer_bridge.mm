// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"

#include "base/logging.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

ChromeIdentityServiceObserverBridge::ChromeIdentityServiceObserverBridge(
    id<ChromeIdentityServiceObserver> observer)
    : observer_(observer), scoped_observer_(this) {
  DCHECK(observer_);
  scoped_observer_.Add(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

ChromeIdentityServiceObserverBridge::~ChromeIdentityServiceObserverBridge() {}

void ChromeIdentityServiceObserverBridge::OnIdentityListChanged() {
  if ([observer_ respondsToSelector:@selector(onIdentityListChanged)])
    [observer_ onIdentityListChanged];
}

void ChromeIdentityServiceObserverBridge::OnAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    ios::AccessTokenErrorReason error) {
  if ([observer_
          respondsToSelector:@selector(onAccessTokenRefreshFailed:error:)]) {
    [observer_ onAccessTokenRefreshFailed:identity error:error];
  }
}

void ChromeIdentityServiceObserverBridge::OnProfileUpdate(
    ChromeIdentity* identity) {
  if ([observer_ respondsToSelector:@selector(onProfileUpdate:)])
    [observer_ onProfileUpdate:identity];
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNSTREAM_CHROMIUM_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_DOWNSTREAM_CHROMIUM_BROWSER_PROVIDER_H_

#include "ios/chrome/browser/providers/chromium_browser_provider.h"

// DownstreamChromiumBrowserProvider contains provider implementations that will
// eventually move into the upstream ChromiumBrowserProvider, but currently
// cannot move because they have internal dependencies.
class DownstreamChromiumBrowserProvider : public ChromiumBrowserProvider {
 public:
  DownstreamChromiumBrowserProvider();
  ~DownstreamChromiumBrowserProvider() override;

 private:
  // ChromeBrowserProvider implementations.  All of these will move upstream
  // into ChromiumBrowserProvider eventually, and from there callers will be
  // converted to not go through the provider API at all.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  ios::LiveTabContextProvider* GetLiveTabContextProvider() override;
  void SetUIViewAlphaWithAnimation(UIView* view, float alpha) override;
  autofill::CardUnmaskPromptView* CreateCardUnmaskPromptView(
      autofill::CardUnmaskPromptController* controller) override;
  bool IsOffTheRecordSessionActive() override;
  void GetFaviconForURL(
      ios::ChromeBrowserState* browser_state,
      const GURL& page_url,
      const std::vector<int>& desired_sizes_in_pixel,
      const favicon_base::FaviconResultsCallback& callback) const override;
  std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
  CreateSyncedWindowDelegatesGetter(
      ios::ChromeBrowserState* browser_state) override;

  std::unique_ptr<ios::LiveTabContextProvider>
      tab_restore_service_delegate_provider_;

  DISALLOW_COPY_AND_ASSIGN(DownstreamChromiumBrowserProvider);
};

#endif  // IOS_CHROME_BROWSER_DOWNSTREAM_CHROMIUM_BROWSER_PROVIDER_H_

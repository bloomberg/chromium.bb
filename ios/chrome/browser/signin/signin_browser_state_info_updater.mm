// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_browser_state_info_updater.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/browser/account_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"

SigninBrowserStateInfoUpdater::SigninBrowserStateInfoUpdater(
    SigninManagerBase* signin_manager,
    SigninErrorController* signin_error_controller,
    const base::FilePath& browser_state_path)
    : signin_error_controller_(signin_error_controller),
      browser_state_path_(browser_state_path),
      signin_error_controller_observer_(this),
      signin_manager_observer_(this) {
  signin_error_controller_observer_.Add(signin_error_controller);
  // TODO(crbug.com/908457): Call OnErrorChanged() here, to catch any change
  // that happened since the construction of SigninErrorController. BrowserState
  // metrics depend on this bug and must be fixed first.
}

SigninBrowserStateInfoUpdater::~SigninBrowserStateInfoUpdater() = default;

void SigninBrowserStateInfoUpdater::Shutdown() {
  signin_error_controller_observer_.RemoveAll();
  signin_manager_observer_.RemoveAll();
}

void SigninBrowserStateInfoUpdater::OnErrorChanged() {
  BrowserStateInfoCache* cache = GetApplicationContext()
                                     ->GetChromeBrowserStateManager()
                                     ->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(browser_state_path_);
  if (index == std::string::npos)
    return;

  cache->SetBrowserStateIsAuthErrorAtIndex(
      index, signin_error_controller_->HasError());
}

void SigninBrowserStateInfoUpdater::GoogleSigninSucceeded(
    const AccountInfo& account_info) {
  ios::ChromeBrowserStateManager* browser_state_manager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  BrowserStateInfoCache* cache =
      browser_state_manager->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(browser_state_path_);

  if (index == std::string::npos)
    return;

  cache->SetAuthInfoOfBrowserStateAtIndex(
      index, account_info.gaia, base::UTF8ToUTF16(account_info.email));
}

void SigninBrowserStateInfoUpdater::GoogleSignedOut(
    const AccountInfo& account_info) {
  BrowserStateInfoCache* cache = GetApplicationContext()
                                     ->GetChromeBrowserStateManager()
                                     ->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(browser_state_path_);

  // If sign out occurs because Sync setup was in progress and the browser state
  // got deleted, then it is no longer in the cache.
  if (index == std::string::npos)
    return;

  cache->SetAuthInfoOfBrowserStateAtIndex(index, /*gaia_id=*/std::string(),
                                          /*user_name=*/base::string16());
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/browser_state_data_remover.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/clear_browsing_data_command.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"

namespace {
const int kRemoveAllDataMask = ~0;
}

BrowserStateDataRemover::BrowserStateDataRemover(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state), forget_last_username_(false) {
  callback_subscription_ =
      IOSChromeBrowsingDataRemover::RegisterOnBrowsingDataRemovedCallback(
          base::Bind(&BrowserStateDataRemover::NotifyWithDetails,
                     base::Unretained(this)));
}

BrowserStateDataRemover::~BrowserStateDataRemover() {
}

void BrowserStateDataRemover::SetForgetLastUsername() {
  forget_last_username_ = true;
}

void BrowserStateDataRemover::RemoveBrowserStateData(ProceduralBlock callback) {
  DCHECK(!callback_);
  callback_.reset([callback copy]);

  base::scoped_nsobject<ClearBrowsingDataCommand> command(
      [[ClearBrowsingDataCommand alloc]
          initWithBrowserState:browser_state_
                          mask:kRemoveAllDataMask]);

  UIWindow* mainWindow = [[UIApplication sharedApplication] keyWindow];
  DCHECK(mainWindow);
  [mainWindow chromeExecuteCommand:command];
}

void BrowserStateDataRemover::NotifyWithDetails(
    const IOSChromeBrowsingDataRemover::NotificationDetails& details) {
  // Remove bookmarks once all browsing data was removed.
  // Removal of browsing data waits for the bookmark model to be loaded, so
  // there should be no issue calling the function here.
  CHECK(RemoveAllUserBookmarksIOS(browser_state_))
      << "Failed to remove all user bookmarks.";

  if (details.removal_mask != kRemoveAllDataMask) {
    NOTREACHED() << "Unexpected partial remove browsing data request "
                 << "(removal mask = " << details.removal_mask << ")";
    return;
  }

  if (forget_last_username_) {
    browser_state_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
    browser_state_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
  }

  if (callback_)
    callback_.get()();

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

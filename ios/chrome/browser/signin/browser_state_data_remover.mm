// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/browser_state_data_remover.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/reading_list_remover_helper.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/clear_browsing_data_command.h"

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

// static
void BrowserStateDataRemover::ClearData(ios::ChromeBrowserState* browser_state,
                                        ProceduralBlock completion) {
  // The user just changed the account and chose to clear the previously
  // existing data. As browsing data is being cleared, it is fine to clear the
  // last username, as there will be no data to be merged.
  BrowserStateDataRemover* remover = new BrowserStateDataRemover(browser_state);
  remover->SetForgetLastUsername();
  remover->RemoveBrowserStateData(completion);
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
                          mask:kRemoveAllDataMask
                    timePeriod:browsing_data::TimePeriod::ALL_TIME]);

  UIWindow* mainWindow = [[UIApplication sharedApplication] keyWindow];
  DCHECK(mainWindow);
  [mainWindow chromeExecuteCommand:command];
}

void BrowserStateDataRemover::NotifyWithDetails(
    const IOSChromeBrowsingDataRemover::NotificationDetails& details) {
  // Remove bookmarks and Reading List entriesonce all browsing data was
  // removed.
  // Removal of browsing data waits for the bookmark model to be loaded, so
  // there should be no issue calling the function here.
  CHECK(RemoveAllUserBookmarksIOS(browser_state_))
      << "Failed to remove all user bookmarks.";
  reading_list_remover_helper_ =
      base::MakeUnique<reading_list::ReadingListRemoverHelper>(browser_state_);
  reading_list_remover_helper_->RemoveAllUserReadingListItemsIOS(
      base::Bind(&BrowserStateDataRemover::ReadingListCleaned,
                 base::Unretained(this), details));
}

void BrowserStateDataRemover::ReadingListCleaned(
    const IOSChromeBrowsingDataRemover::NotificationDetails& details,
    bool reading_list_cleaned) {
  CHECK(reading_list_cleaned)
      << "Failed to remove all user reading list items.";

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

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

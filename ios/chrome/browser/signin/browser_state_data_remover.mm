// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/browser_state_data_remover.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/clear_browsing_data_command.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"

using bookmarks::BookmarkNode;

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
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(browser_state_);
  DCHECK(bookmarkModel->loaded())
      << "Sync is started lazily by the profile once the bookmark model is "
      << "loaded. Expecting the bookmark model to be loaded when the user "
      << "attempts to swap his accounts.";
  bookmarkModel->RemoveAllUserBookmarks();

  bookmarks::BookmarkClient* client = bookmarkModel->client();
  const BookmarkNode* root = bookmarkModel->root_node();
  for (int i = 0; i < root->child_count(); ++i) {
    // Check that remaining bookmarks are all not user-editable.
    if (!client->CanBeEditedByUser(root->GetChild(i)))
      continue;
    CHECK(root->GetChild(i)->empty()) << "Failed to remove all user bookmarks.";
  }

  if (details.removal_mask != kRemoveAllDataMask) {
    NOTREACHED() << "Unexpected partial remove browsing data request "
                 << "(removal mask = " << details.removal_mask << ")";
    return;
  }

  if (forget_last_username_)
    browser_state_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);

  if (callback_)
    callback_.get()();

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

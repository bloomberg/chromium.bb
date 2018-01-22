// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"

#include "build/buildflag.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/signin/core/browser/signin_features.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/signin_ui_util.h"
#endif

BookmarkBubbleSignInDelegate::BookmarkBubbleSignInDelegate(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()) {
  BrowserList::AddObserver(this);
}

BookmarkBubbleSignInDelegate::~BookmarkBubbleSignInDelegate() {
  BrowserList::RemoveObserver(this);
}

void BookmarkBubbleSignInDelegate::ShowBrowserSignin() {
  EnsureBrowser();
  chrome::ShowBrowserSignin(
      browser_, signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void BookmarkBubbleSignInDelegate::EnableSync(const AccountInfo& account) {
  EnsureBrowser();
  signin_ui_util::EnableSync(
      browser_, account,
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE);

  // TODO(msarda): Close the bookmarks bubble once the enable sync flow has
  // started.
}
#endif

void BookmarkBubbleSignInDelegate::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = NULL;
}

void BookmarkBubbleSignInDelegate::EnsureBrowser() {
  if (!browser_) {
    Profile* original_profile = profile_->GetOriginalProfile();
    browser_ = chrome::FindLastActiveWithProfile(original_profile);
    if (!browser_) {
      browser_ = new Browser(Browser::CreateParams(original_profile, true));
    }
  }
}

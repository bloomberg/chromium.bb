// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/toolbar/toolbar_model_impl_ios.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/toolbar/toolbar_model_impl.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#include "ios/chrome/browser/tabs/tab.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_delegate_ios.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const size_t kMaxURLDisplayChars = 32 * 1024;

bookmarks::BookmarkModel* GetBookmarkModelForWebState(
    web::WebState* web_state) {
  if (!web_state)
    return nullptr;
  web::BrowserState* browser_state = web_state->GetBrowserState();
  if (!browser_state)
    return nullptr;
  return ios::BookmarkModelFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(browser_state));
}
}  // namespace

ToolbarModelImplIOS::ToolbarModelImplIOS(ToolbarModelDelegateIOS* delegate) {
  delegate_ = delegate;
  toolbar_model_.reset(new ToolbarModelImpl(delegate, kMaxURLDisplayChars));
}

ToolbarModelImplIOS::~ToolbarModelImplIOS() {}

ToolbarModel* ToolbarModelImplIOS::GetToolbarModel() {
  return toolbar_model_.get();
}

bool ToolbarModelImplIOS::IsLoading() {
  // Please note, ToolbarModel's notion of isLoading is slightly different from
  // WebState's IsLoading().
  web::WebState* web_state = delegate_->GetActiveWebState();
  return web_state && web_state->IsLoading() && !IsCurrentTabNativePage();
}

CGFloat ToolbarModelImplIOS::GetLoadProgressFraction() {
  web::WebState* webState = delegate_->GetActiveWebState();
  return webState ? webState->GetLoadingProgress() : 0.0;
}

bool ToolbarModelImplIOS::CanGoBack() {
  if (!delegate_)
    return false;
  web::WebState* web_state = delegate_->GetActiveWebState();
  return web_state && web_state->GetNavigationManager()->CanGoBack();
}

bool ToolbarModelImplIOS::CanGoForward() {
  if (!delegate_)
    return false;
  web::WebState* web_state = delegate_->GetActiveWebState();
  return web_state && web_state->GetNavigationManager()->CanGoForward();
}

bool ToolbarModelImplIOS::IsCurrentTabNativePage() {
  web::WebState* web_state = delegate_->GetActiveWebState();
  return web_state &&
         web_state->GetLastCommittedURL().SchemeIs(kChromeUIScheme);
}

bool ToolbarModelImplIOS::IsCurrentTabBookmarked() {
  web::WebState* web_state = delegate_->GetActiveWebState();
  bookmarks::BookmarkModel* bookmarkModel =
      GetBookmarkModelForWebState(web_state);
  return web_state && bookmarkModel &&
         bookmarkModel->IsBookmarked(web_state->GetLastCommittedURL());
}

bool ToolbarModelImplIOS::IsCurrentTabBookmarkedByUser() {
  web::WebState* web_state = delegate_->GetActiveWebState();
  bookmarks::BookmarkModel* bookmarkModel =
      GetBookmarkModelForWebState(web_state);
  return web_state && bookmarkModel &&
         bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
             web_state->GetLastCommittedURL());
}

bool ToolbarModelImplIOS::ShouldDisplayHintText() {
  web::WebState* web_state = delegate_->GetActiveWebState();
  if (!web_state)
    return false;

  Tab* tab = LegacyTabHelper::GetTabForWebState(web_state);
  return tab && [tab.webController wantsLocationBarHintText];
}

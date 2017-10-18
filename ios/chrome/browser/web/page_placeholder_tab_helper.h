// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_TAB_HELPER_H_

#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@protocol PagePlaceholderTabHelperDelegate;

// Displays placeholder to cover what WebState is actually displaying. Can be
// used to display the cached image of the web page during the Tab restoration.
class PagePlaceholderTabHelper
    : public web::WebStateUserData<PagePlaceholderTabHelper>,
      public web::WebStateObserver {
 public:
  ~PagePlaceholderTabHelper() override;

  // Creates TabHelper. |delegate| is not retained by TabHelper and must not be
  // null.
  static void CreateForWebState(web::WebState* web_state,
                                id<PagePlaceholderTabHelperDelegate> delegate);

  // Displays placeholder between DidStartNavigation and PageLoaded
  // WebStateObserver callbacks. If navigation takes too long, then placeholder
  // will be removed before navigation is finished.
  void AddPlaceholderForNextNavigation();

  // true if placeholder will be displayed between DidStartNavigation and
  // PageLoaded WebStateObserver callbacks.
  bool will_add_placeholder_for_next_navigation() {
    return add_placeholder_for_next_navigation_;
  }

 private:
  PagePlaceholderTabHelper(web::WebState* web_state,
                           id<PagePlaceholderTabHelperDelegate> delegate);

  // web::WebStateObserver overrides:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  void AddPlaceholder();
  void RemovePlaceholder();

  // Delegate which displays and removes the placeholder to cover WebState's
  // view.
  __weak id<PagePlaceholderTabHelperDelegate> delegate_ = nil;

  // true if placeholder is currently being displayed.
  bool displaying_placeholder_ = false;

  bool add_placeholder_for_next_navigation_ = false;

  base::WeakPtrFactory<PagePlaceholderTabHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PagePlaceholderTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_TAB_HELPER_H_

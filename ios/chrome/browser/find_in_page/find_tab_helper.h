// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_TAB_HELPER_H_

#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@class FindInPageController;
@protocol FindInPageControllerDelegate;

// Adds support for the "Find in page" feature.
class FindTabHelper : public web::WebStateObserver,
                      public web::WebStateUserData<FindTabHelper> {
 public:
  // Creates a FindTabHelper and attaches it to the given |web_state|.
  // |controller_delegate| can be nil.
  static void CreateForWebState(
      web::WebState* web_state,
      id<FindInPageControllerDelegate> controller_delegate);

  // Returns the find in page controller.
  FindInPageController* GetController();

 private:
  FindTabHelper(web::WebState* web_state,
                id<FindInPageControllerDelegate> controller_delegate);
  ~FindTabHelper() override;

  // web::WebStateObserver.
  void NavigationItemCommitted(
      const web::LoadCommittedDetails& load_details) override;
  void WebStateDestroyed() override;

  // The ObjC find in page controller.
  FindInPageController* controller_;

  DISALLOW_COPY_AND_ASSIGN(FindTabHelper);
};

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_TAB_HELPER_H_

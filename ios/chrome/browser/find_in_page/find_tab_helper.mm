// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_tab_helper.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(FindTabHelper);

// static
void FindTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<FindInPageControllerDelegate> controller_delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(), base::WrapUnique(new FindTabHelper(
                                              web_state, controller_delegate)));
  }
}

FindTabHelper::FindTabHelper(
    web::WebState* web_state,
    id<FindInPageControllerDelegate> controller_delegate)
    : web::WebStateObserver(web_state) {
  controller_.reset([[FindInPageController alloc]
      initWithWebState:web_state
              delegate:controller_delegate]);
}

FindTabHelper::~FindTabHelper() {}

void FindTabHelper::StartFinding(NSString* search_term,
                                 FindInPageCompletionBlock completion) {
  [controller_ findStringInPage:search_term
              completionHandler:^{
                FindInPageModel* model = controller_.get().findInPageModel;
                completion(model);
              }];
}

void FindTabHelper::ContinueFinding(FindDirection direction,
                                    FindInPageCompletionBlock completion) {
  FindInPageModel* model = controller_.get().findInPageModel;

  if (direction == FORWARD) {
    [controller_ findNextStringInPageWithCompletionHandler:^{
      completion(model);
    }];

  } else if (direction == REVERSE) {
    [controller_ findPreviousStringInPageWithCompletionHandler:^{
      completion(model);
    }];

  } else {
    NOTREACHED();
  }
}

void FindTabHelper::StopFinding(ProceduralBlock completion) {
  SetFindUIActive(false);
  [controller_ disableFindInPageWithCompletionHandler:completion];
}

FindInPageModel* FindTabHelper::GetFindResult() const {
  return controller_.get().findInPageModel;
}

bool FindTabHelper::CurrentPageSupportsFindInPage() const {
  return [controller_ canFindInPage];
}

bool FindTabHelper::IsFindUIActive() const {
  return controller_.get().findInPageModel.enabled;
}

void FindTabHelper::SetFindUIActive(bool active) {
  controller_.get().findInPageModel.enabled = active;
}

void FindTabHelper::PersistSearchTerm() {
  [controller_ saveSearchTerm];
}

void FindTabHelper::RestoreSearchTerm() {
  [controller_ restoreSearchTerm];
}

void FindTabHelper::NavigationItemCommitted(
    const web::LoadCommittedDetails& load_details) {
  StopFinding(nil);
}

void FindTabHelper::WebStateDestroyed() {
  [controller_ detachFromWebState];
}

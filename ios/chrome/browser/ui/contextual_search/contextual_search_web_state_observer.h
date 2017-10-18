// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_STATE_OBSERVER_H_

#import <UIKit/UIKit.h>

#include "ios/web/public/web_state/web_state_observer.h"

@protocol ContextualSearchWebStateDelegate<NSObject>
@optional
- (void)webState:(web::WebState*)webState
    navigatedWithDetails:(const web::LoadCommittedDetails&)details;
- (void)webState:(web::WebState*)webState
    pageLoadedWithStatus:(web::PageLoadCompletionStatus)loadStatus;
- (void)webStateDestroyed:(web::WebState*)webState;

@end

class ContextualSearchWebStateObserver : public web::WebStateObserver {
 public:
  ContextualSearchWebStateObserver(
      id<ContextualSearchWebStateDelegate> delegate);
  ~ContextualSearchWebStateObserver() override;

  void ObserveWebState(web::WebState* web_state);

 private:
  void NavigationItemCommitted(
      web::WebState* web_state,
      const web::LoadCommittedDetails& load_details) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  id<ContextualSearchWebStateDelegate> delegate_;
  bool loaded_;
};

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_STATE_OBSERVER_H_

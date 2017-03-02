// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_OBSERVER_BRIDGE_H_
#define IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_observer.h"

// Protocol that correspond to WebStateListObserver API. Allows registering
// Objective-C objects to listen to WebStateList events.
@protocol WebStateListObserving<NSObject>

@optional

// Invoked after a new WebState has been added to the WebStateList at the
// specified index.
- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index;

// Invoked after the WebState at the specified index is moved to another index.
- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex;

// Invoked after the WebState at the specified index is replaced by another
// WebState.
- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index;

// Invoked after the WebState at the specified index has been detached. The
// WebState is still valid but is no longer in the WebStateList.
- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index;

// Invoked after |newWebState| was activated at the specified index. Both
// WebState are either valid or null (if there was no selection or there is
// no selection). If the change is due to an user action, |userAction| will
// be true.
- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction;

@end

// Observer that bridges WebStateList events to an Objective-C observer that
// implements the WebStateListObserver protocol (the observer is owned).
class WebStateListObserverBridge : public WebStateListObserver {
 public:
  explicit WebStateListObserverBridge(id<WebStateListObserving> observer);
  ~WebStateListObserverBridge() override;

 private:
  // WebStateListObserver implementation.
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;
  void WebStateMoved(WebStateList* web_state_list,
                     web::WebState* web_state,
                     int from_index,
                     int to_index) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           bool user_action) override;

  id<WebStateListObserving> observer_;

  DISALLOW_COPY_AND_ASSIGN(WebStateListObserverBridge);
};

#endif  // IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_OBSERVER_BRIDGE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory_coordinator.h"

#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryCoordinator ()<WebStateListObserving>

// The View Controller for the input accessory view.
@property FormInputAccessoryViewController* formInputAccessoryViewController;

@end

@implementation FormInputAccessoryCoordinator {
  // The WebStateList this instance is observing in order to update the
  // active WebState.
  WebStateList* _webStateList;

  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

@synthesize formInputAccessoryViewController =
    _formInputAccessoryViewController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    DCHECK(webStateList);
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
  return self;
}

- (void)dealloc {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
}

- (void)start {
  self.formInputAccessoryViewController =
      [[FormInputAccessoryViewController alloc] init];
  [self updateFormInputAccessoryViewController];
}

- (void)stop {
  self.formInputAccessoryViewController = nil;
}

#pragma mark - Private

- (void)updateFormInputAccessoryViewController {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  self.formInputAccessoryViewController.webState = activeWebState;
}

#pragma mark - CRWWebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  self.formInputAccessoryViewController.webState = newWebState;
}

@end

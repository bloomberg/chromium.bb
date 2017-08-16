// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_search_commands.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_consumer.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FindInPageMediator ()<FindInPageSearchCommands,
                                 WebStateListObserving>

// The WebStateList that is being observed by this mediator.
@property(nonatomic, assign) WebStateList* webStateList;

// Provides the consumer that will be updated by this mediator.
@property(nonatomic, weak) id<FindInPageConsumerProvider> provider;

// Used to dispatch commands.
@property(nonatomic, weak) id<FindInPageVisibilityCommands> dispatcher;

// Called by the Find in Page backend when new results are available.
- (void)findResultsAvailable:(FindInPageModel*)model;

@end

@implementation FindInPageMediator {
  // Observes the WebStateList so that this mediator can update the UI when the
  // active WebState changes.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserverBridge>>
      _scopedWebStateListObserver;
}

@synthesize dispatcher = _dispatcher;
@synthesize provider = _provider;
@synthesize webStateList = _webStateList;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            provider:(id<FindInPageConsumerProvider>)provider
                          dispatcher:
                              (id<FindInPageVisibilityCommands>)dispatcher {
  if ((self = [super init])) {
    DCHECK(webStateList);
    DCHECK(provider);

    _webStateList = webStateList;
    _provider = provider;
    _dispatcher = dispatcher;

    _webStateListObserver = base::MakeUnique<WebStateListObserverBridge>(self);
    _scopedWebStateListObserver = base::MakeUnique<
        ScopedObserver<WebStateList, WebStateListObserverBridge>>(
        _webStateListObserver.get());
    _scopedWebStateListObserver->Add(_webStateList);
  }
  return self;
}

- (void)stopFinding {
  web::WebState* webState = self.webStateList->GetActiveWebState();
  if (webState) {
    FindTabHelper* helper = FindTabHelper::FromWebState(webState);
    DCHECK(helper);
    helper->StopFinding(nil);
  }
}

- (void)findResultsAvailable:(FindInPageModel*)model {
  id<FindInPageConsumer> consumer = self.provider.consumer;
  [consumer setCurrentMatch:model.currentIndex ofTotalMatches:model.matches];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction {
  // Update the visibility of the find bar based on the |newWebState|.
  if (newWebState) {
    FindTabHelper* helper = FindTabHelper::FromWebState(newWebState);
    DCHECK(helper);

    if (helper->IsFindUIActive()) {
      [self.dispatcher showFindInPage];
    } else {
      [self.dispatcher hideFindInPage];
    }
  } else {
    // If there is no new active WebState, hide the find bar.
    [self.dispatcher hideFindInPage];
  }
}

#pragma mark - Command handlers

- (void)findStringInPage:(NSString*)searchTerm {
  web::WebState* webState = self.webStateList->GetActiveWebState();

  FindTabHelper* helper = FindTabHelper::FromWebState(webState);
  DCHECK(helper);
  helper->StartFinding(searchTerm, ^(FindInPageModel* model) {
    [self findResultsAvailable:model];
  });
}

- (void)findNextInPage {
  web::WebState* webState = self.webStateList->GetActiveWebState();
  FindTabHelper* helper = FindTabHelper::FromWebState(webState);
  DCHECK(helper);
  helper->ContinueFinding(FindTabHelper::FORWARD, ^(FindInPageModel* model) {
    [self findResultsAvailable:model];
  });
}

- (void)findPreviousInPage {
  web::WebState* webState = self.webStateList->GetActiveWebState();
  FindTabHelper* helper = FindTabHelper::FromWebState(webState);
  DCHECK(helper);
  helper->ContinueFinding(FindTabHelper::REVERSE, ^(FindInPageModel* model) {
    [self findResultsAvailable:model];
  });
}

@end

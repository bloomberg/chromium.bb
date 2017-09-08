// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/root_coordinator_initializer.h"

#import "ios/chrome/app/startup/provider_registration.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/browser_list/browser_list.h"
#import "ios/chrome/browser/ui/browser_list/browser_list_factory.h"
#import "ios/chrome/browser/ui/browser_list/browser_list_session_service.h"
#import "ios/chrome/browser/ui/browser_list/browser_list_session_service_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/clean/chrome/app/steps/step_context.h"
#import "ios/clean/chrome/app/steps/step_features.h"
#import "ios/clean/chrome/browser/ui/root/root_coordinator.h"
#include "ios/web/public/web_state/web_state.h"

@protocol StepContext;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation RootCoordinatorInitializer {
  RootCoordinator* _rootCoordinator;
}
- (instancetype)init {
  if ((self = [super init])) {
    self.providedFeature = step_features::kRootCoordinatorStarted;
    self.requiredFeatures = @[ step_features::kMainWindow ];
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  _rootCoordinator = [[RootCoordinator alloc] init];

  // Create the main browser, set the root coordinator's dispatcher.
  _rootCoordinator.browser =
      BrowserListFactory::GetForBrowserState(context.browserState)
          ->CreateNewBrowser();
  _rootCoordinator.dispatcher = [[CommandDispatcher alloc] init];

  // Restore the previous session's tabs, if any.
  BrowserListSessionService* service =
      BrowserListSessionServiceFactory::GetForBrowserState(
          context.browserState);

  if (!service || !service->RestoreSession()) {
    WebStateList& webStateList = _rootCoordinator.browser->web_state_list();
    web::WebState::CreateParams webStateCreateParams(
        _rootCoordinator.browser->browser_state());
    webStateList.InsertWebState(0, web::WebState::Create(webStateCreateParams),
                                WebStateList::INSERT_NO_FLAGS,
                                WebStateOpener());
    webStateList.ActivateWebStateAt(0);
  }

  // Start the root coordinator and assign it as the URL opener for the app.
  // This creates and starts the Chrome UI.
  [_rootCoordinator start];
  context.URLOpener = _rootCoordinator;
  context.window.rootViewController = _rootCoordinator.viewController;

  // Size the main view controller to fit the whole screen.
  [_rootCoordinator.viewController.view setFrame:context.window.bounds];
  context.window.hidden = NO;
}

@end

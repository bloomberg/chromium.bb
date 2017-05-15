// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/root_coordinator+application_step.h"

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/clean/chrome/app/application_state.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser_list.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser_list_session_service.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser_list_session_service_factory.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// SupportsUserData storage container for a BrowserCoordinator object, which
// keeps a strong reference to the object it is initialized with.
class RootCoordinatorContainer : public base::SupportsUserData::Data {
 public:
  explicit RootCoordinatorContainer(BrowserCoordinator* coordinator)
      : coordinator_(coordinator) {}

 private:
  // Mark this variable unused to avoid a compiler warning; the compiler
  // doesn't have visibility into the ARC-injected retain/release.
  BrowserCoordinator* __attribute__((unused)) coordinator_;
};

const char kRootCoordinatorContainerKey[] = "root_coordinator";
}  // namespace

@interface StopRootCoordinator : NSObject<ApplicationStep>
@end

@implementation RootCoordinator (ApplicationStep)

- (BOOL)canRunInState:(ApplicationState*)state {
  return [state.window isKeyWindow] && state.phase == APPLICATION_FOREGROUNDED;
}

- (void)runInState:(ApplicationState*)state {
  self.browser =
      BrowserList::FromBrowserState(state.browserState)->CreateNewBrowser();

  BrowserListSessionService* service =
      BrowserListSessionServiceFactory::GetForBrowserState(state.browserState);

  if (!service || !service->RestoreSession()) {
    WebStateList& webStateList = self.browser->web_state_list();
    web::WebState::CreateParams webStateCreateParams(
        self.browser->browser_state());
    webStateList.InsertWebState(0, web::WebState::Create(webStateCreateParams));
    webStateList.ActivateWebStateAt(0);
  }

  [self start];

  state.window.rootViewController = self.viewController;

  // Size the main view controller to fit the whole screen.
  [self.viewController.view setFrame:state.window.bounds];

  // Tell the application state to use this object to open URLs.
  state.URLOpener = self;

  // Show the window.
  state.window.hidden = NO;

  // Make sure this object stays alive.
  state.persistentState->SetUserData(
      kRootCoordinatorContainerKey,
      base::MakeUnique<RootCoordinatorContainer>(self));

  // Add a termination step to remove this object.
  [[state terminationSteps] addObject:[[StopRootCoordinator alloc] init]];
}

@end

@implementation StopRootCoordinator

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.phase = APPLICATION_TERMINATING;
}

- (void)runInState:(ApplicationState*)state {
  state.persistentState->RemoveUserData(kRootCoordinatorContainerKey);
}

@end

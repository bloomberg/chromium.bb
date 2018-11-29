// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_coordinator.h"

#include <memory>

#include "base/scoped_observer.h"
#import "ios/chrome/browser/app_launcher/app_launcher_abuse_detector.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/alert_coordinator/repost_form_coordinator.h"
#import "ios/chrome/browser/ui/app_launcher/app_launcher_coordinator.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory_coordinator.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/download/pass_kit_coordinator.h"
#import "ios/chrome/browser/ui/page_info/page_info_legacy_coordinator.h"
#import "ios/chrome/browser/ui/print/print_controller.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_legacy_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"
#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserCoordinator () <FormInputAccessoryCoordinatorDelegate,
                                  RepostFormTabHelperDelegate,
                                  WebStateListObserving>

// Handles command dispatching.
@property(nonatomic, strong) CommandDispatcher* dispatcher;

// =================================================
// Child Coordinators, listed in alphabetical order.
// =================================================

// Coordinator for UI related to launching external apps.
@property(nonatomic, strong) AppLauncherCoordinator* appLauncherCoordinator;

// Coordinator in charge of the presenting autofill options above the
// keyboard.
@property(nonatomic, strong)
    FormInputAccessoryCoordinator* formInputAccessoryCoordinator;

// Coordinator for Page Info UI.
@property(nonatomic, strong) PageInfoLegacyCoordinator* pageInfoCoordinator;

// Coordinator for the PassKit UI presentation.
@property(nonatomic, strong) PassKitCoordinator* passKitCoordinator;

// Used to display the Print UI. Nil if not visible.
// TODO(crbug.com/910017): Convert to coordinator.
@property(nonatomic, strong) PrintController* printController;

// Coordinator for the QR scanner.
@property(nonatomic, strong) QRScannerLegacyCoordinator* qrScannerCoordinator;

// Coordinator for displaying the Reading List.
@property(nonatomic, strong) ReadingListCoordinator* readingListCoordinator;

// Coordinator for Recent Tabs.
@property(nonatomic, strong) RecentTabsCoordinator* recentTabsCoordinator;

// Coordinator for displaying Repost Form dialog.
@property(nonatomic, strong) RepostFormCoordinator* repostFormCoordinator;

// Coordinator for displaying snackbars.
@property(nonatomic, strong) SnackbarCoordinator* snackbarCoordinator;

// Coordinator for presenting SKStoreProductViewController.
@property(nonatomic, strong) StoreKitCoordinator* storeKitCoordinator;

@end

@implementation BrowserCoordinator {
  // Observers for WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browserState);
  DCHECK(!self.viewController);
  self.dispatcher = [[CommandDispatcher alloc] init];
  [self createViewController];
  [self startChildCoordinators];
  [self.dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(BrowserCoordinatorCommands)];
  [self installDelegatesForAllWebStates];
  [self addWebStateListObserver];
  [super start];
}

- (void)stop {
  [super stop];
  [self removeWebStateListObserver];
  [self uninstallDelegatesForAllWebStates];
  [self.dispatcher stopDispatchingToTarget:self];
  [self stopChildCoordinators];
  [self destroyViewController];
  self.dispatcher = nil;
}

#pragma mark - Public

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self.passKitCoordinator stop];

  [self.printController dismissAnimated:YES];
  self.printController = nil;

  [self.viewController clearPresentedStateWithCompletion:completion
                                          dismissOmnibox:dismissOmnibox];
}

#pragma mark - Private

// Instantiates a BrowserViewController.
- (void)createViewController {
  BrowserViewControllerDependencyFactory* factory =
      [[BrowserViewControllerDependencyFactory alloc]
          initWithBrowserState:self.browserState
                  webStateList:self.tabModel.webStateList];
  _viewController = [[BrowserViewController alloc]
                initWithTabModel:self.tabModel
                    browserState:self.browserState
               dependencyFactory:factory
      applicationCommandEndpoint:self.applicationCommandHandler
               commandDispatcher:self.dispatcher];
}

// Shuts down the BrowserViewController.
- (void)destroyViewController {
  [self.viewController browserStateDestroyed];
  [self.viewController shutdown];
  _viewController = nil;
}

// Starts child coordinators.
- (void)startChildCoordinators {
  // Dispatcher should be instantiated so that it can be passed to child
  // coordinators.
  DCHECK(self.dispatcher);

  self.appLauncherCoordinator = [[AppLauncherCoordinator alloc]
      initWithBaseViewController:self.viewController];

  self.formInputAccessoryCoordinator = [[FormInputAccessoryCoordinator alloc]
      initWithBaseViewController:self.viewController
                    browserState:self.browserState
                    webStateList:self.tabModel.webStateList];
  self.formInputAccessoryCoordinator.delegate = self;
  [self.formInputAccessoryCoordinator start];

  self.pageInfoCoordinator = [[PageInfoLegacyCoordinator alloc]
      initWithBaseViewController:self.viewController
                    browserState:self.browserState];
  self.pageInfoCoordinator.dispatcher = self.dispatcher;
  self.pageInfoCoordinator.loader = self.viewController;
  self.pageInfoCoordinator.presentationProvider = self.viewController;
  self.pageInfoCoordinator.tabModel = self.tabModel;

  self.passKitCoordinator = [[PassKitCoordinator alloc]
      initWithBaseViewController:self.viewController];

  /* PrintController is created and started by a BrowserCommand */

  self.qrScannerCoordinator = [[QRScannerLegacyCoordinator alloc]
      initWithBaseViewController:self.viewController];
  self.qrScannerCoordinator.dispatcher = self.dispatcher;

  /* ReadingListCoordinator is created and started by a BrowserCommand */

  /* RecentTabsCoordinator is created and started by a BrowserCommand */

  /* RepostFormCoordinator is created and started by a delegate method */

  self.snackbarCoordinator = [[SnackbarCoordinator alloc] init];
  self.snackbarCoordinator.dispatcher = self.dispatcher;
  [self.snackbarCoordinator start];

  self.storeKitCoordinator = [[StoreKitCoordinator alloc]
      initWithBaseViewController:self.viewController];
}

// Stops child coordinators.
- (void)stopChildCoordinators {
  // TODO(crbug.com/906541) : AppLauncherCoordinator is not a subclass of
  // ChromeCoordinator, and does not have a |-stop| method.
  self.appLauncherCoordinator = nil;

  [self.formInputAccessoryCoordinator stop];
  self.formInputAccessoryCoordinator = nil;

  [self.pageInfoCoordinator stop];
  self.pageInfoCoordinator = nil;

  [self.passKitCoordinator stop];
  self.passKitCoordinator = nil;

  self.printController = nil;

  [self.qrScannerCoordinator stop];
  self.qrScannerCoordinator = nil;

  [self.readingListCoordinator stop];
  self.readingListCoordinator = nil;

  [self.recentTabsCoordinator stop];
  self.recentTabsCoordinator = nil;

  [self.repostFormCoordinator stop];
  self.repostFormCoordinator = nil;

  [self.snackbarCoordinator stop];
  self.snackbarCoordinator = nil;

  [self.storeKitCoordinator stop];
  self.storeKitCoordinator = nil;
}

#pragma mark - BrowserCoordinatorCommands

- (void)printTab {
  if (!self.printController) {
    self.printController = [[PrintController alloc]
        initWithContextGetter:self.browserState->GetRequestContext()];
  }
  Tab* currentTab = self.tabModel.currentTab;
  [self.printController printView:[currentTab viewForPrinting]
                        withTitle:tab_util::GetTabTitle(currentTab.webState)
                   viewController:self.viewController];
}

- (void)showReadingList {
  self.readingListCoordinator = [[ReadingListCoordinator alloc]
      initWithBaseViewController:self.viewController
                    browserState:self.browserState
                          loader:self.viewController];
  [self.readingListCoordinator start];
}

- (void)showRecentTabs {
  // TODO(crbug.com/825431): If BVC's clearPresentedState is ever called (such
  // as in tearDown after a failed egtest), then this coordinator is left in a
  // started state even though its corresponding VC is no longer on screen.
  // That causes issues when the coordinator is started again and we destroy the
  // old mediator without disconnecting it first.  Temporarily work around these
  // issues by not having a long lived coordinator.  A longer-term solution will
  // require finding a way to stop this coordinator so that the mediator is
  // properly disconnected and destroyed and does not live longer than its
  // associated VC.
  if (self.recentTabsCoordinator) {
    [self.recentTabsCoordinator stop];
    self.recentTabsCoordinator = nil;
  }

  self.recentTabsCoordinator = [[RecentTabsCoordinator alloc]
      initWithBaseViewController:self.viewController
                    browserState:self.browserState];
  self.recentTabsCoordinator.loader = self.viewController;
  self.recentTabsCoordinator.dispatcher = self.applicationCommandHandler;
  [self.recentTabsCoordinator start];
}

#pragma mark - FormInputAccessoryCoordinatorDelegate

- (void)openPasswordSettings {
  [self.applicationCommandHandler
      showSavedPasswordsSettingsFromViewController:self.viewController];
}

- (void)openAddressSettings {
  [self.applicationCommandHandler
      showProfileSettingsFromViewController:self.viewController];
}

- (void)openCreditCardSettings {
  [self.applicationCommandHandler
      showCreditCardSettingsFromViewController:self.viewController];
}

#pragma mark - RepostFormTabHelperDelegate

- (void)repostFormTabHelper:(RepostFormTabHelper*)helper
    presentRepostFormDialogForWebState:(web::WebState*)webState
                         dialogAtPoint:(CGPoint)location
                     completionHandler:(void (^)(BOOL))completion {
  self.repostFormCoordinator = [[RepostFormCoordinator alloc]
      initWithBaseViewController:self.viewController
                  dialogLocation:location
                        webState:webState
               completionHandler:completion];
  [self.repostFormCoordinator start];
}

- (void)repostFormTabHelperDismissRepostFormDialog:
    (RepostFormTabHelper*)helper {
  [self.repostFormCoordinator stop];
  self.repostFormCoordinator = nil;
}

// TODO(crbug.com/906525) : Move WebStateListObserving out of
// BrowserCoordinator.
#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self installDelegatesForWebState:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self uninstallDelegatesForWebState:webState];
}

// TODO(crbug.com/906525) : Move out of BrowserCoordinator along with
// WebStateListObserving.
#pragma mark - Private WebState management methods

// Adds observer for WebStateList.
- (void)addWebStateListObserver {
  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver =
      std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
          _webStateListObserverBridge.get());
  _scopedWebStateListObserver->Add(self.tabModel.webStateList);
}

// Removes observer for WebStateList.
- (void)removeWebStateListObserver {
  _scopedWebStateListObserver.reset();
  _webStateListObserverBridge.reset();
}

// Installs delegates for each WebState in WebStateList.
- (void)installDelegatesForAllWebStates {
  for (int i = 0; i < self.tabModel.webStateList->count(); i++) {
    web::WebState* webState = self.tabModel.webStateList->GetWebStateAt(i);
    [self installDelegatesForWebState:webState];
  }
}

// Uninstalls delegates for each WebState in WebStateList.
- (void)uninstallDelegatesForAllWebStates {
  for (int i = 0; i < self.tabModel.webStateList->count(); i++) {
    web::WebState* webState = self.tabModel.webStateList->GetWebStateAt(i);
    [self uninstallDelegatesForWebState:webState];
  }
}

// Install delegates for |webState|.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  AppLauncherTabHelper::CreateForWebState(
      webState, [[AppLauncherAbuseDetector alloc] init],
      self.appLauncherCoordinator);

  PassKitTabHelper::CreateForWebState(webState, self.passKitCoordinator);

  RepostFormTabHelper::CreateForWebState(webState, self);

  if (StoreKitTabHelper::FromWebState(webState)) {
    StoreKitTabHelper::FromWebState(webState)->SetLauncher(
        self.storeKitCoordinator);
  }
}

// Uninstalls delegates for |webState|.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  if (StoreKitTabHelper::FromWebState(webState)) {
    StoreKitTabHelper::FromWebState(webState)->SetLauncher(nil);
  }
}

@end

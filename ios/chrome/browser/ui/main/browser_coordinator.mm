// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_coordinator.h"

#import "ios/chrome/browser/ui/autofill/form_input_accessory_coordinator.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_legacy_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"
#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserCoordinator ()<FormInputAccessoryCoordinatorDelegate>

// Handles command dispatching.
@property(nonatomic, strong) CommandDispatcher* dispatcher;

// =================================================
// Child Coordinators, listed in alphabetical order.
// =================================================

// Coordinator in charge of the presenting autofill options above the
// keyboard.
@property(nonatomic, strong)
    FormInputAccessoryCoordinator* formInputAccessoryCoordinator;

// Coordinator for the QR scanner.
@property(nonatomic, strong) QRScannerLegacyCoordinator* qrScannerCoordinator;

// Coordinator for displaying the Reading List.
@property(nonatomic, strong) ReadingListCoordinator* readingListCoordinator;

// Coordinator for displaying snackbars.
@property(nonatomic, strong) SnackbarCoordinator* snackbarCoordinator;

@end

@implementation BrowserCoordinator
@synthesize dispatcher = _dispatcher;
// Child coordinators
@synthesize formInputAccessoryCoordinator = _formInputAccessoryCoordinator;
@synthesize qrScannerCoordinator = _qrScannerCoordinator;
@synthesize readingListCoordinator = _readingListCoordinator;
@synthesize snackbarCoordinator = _snackbarCoordinator;

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
  [super start];
}

- (void)stop {
  [super stop];
  [self.dispatcher stopDispatchingToTarget:self];
  [self stopChildCoordinators];
  [self destroyViewController];
  self.dispatcher = nil;
}

#pragma mark - Private

// Instantiates a BrowserViewController.
- (void)createViewController {
  BrowserViewControllerDependencyFactory* factory =
      [[BrowserViewControllerDependencyFactory alloc]
          initWithBrowserState:self.browserState
                  webStateList:[self.tabModel webStateList]];
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
  self.formInputAccessoryCoordinator = [[FormInputAccessoryCoordinator alloc]
      initWithBaseViewController:self.viewController
                    browserState:self.browserState
                    webStateList:self.tabModel.webStateList];
  self.formInputAccessoryCoordinator.delegate = self;
  [self.formInputAccessoryCoordinator start];

  self.qrScannerCoordinator = [[QRScannerLegacyCoordinator alloc]
      initWithBaseViewController:self.viewController];
  self.qrScannerCoordinator.dispatcher = self.dispatcher;

  /* ReadingListCoordinator is created and started by a BrowserCommand */

  self.snackbarCoordinator = [[SnackbarCoordinator alloc] init];
  self.snackbarCoordinator.dispatcher = self.dispatcher;
  [self.snackbarCoordinator start];
}

// Stops child coordinators.
- (void)stopChildCoordinators {
  [self.formInputAccessoryCoordinator stop];
  self.formInputAccessoryCoordinator = nil;

  [self.qrScannerCoordinator stop];
  self.qrScannerCoordinator = nil;

  [self.readingListCoordinator stop];
  self.readingListCoordinator = nil;

  [self.snackbarCoordinator stop];
  self.snackbarCoordinator = nil;
}

#pragma mark - BrowserCoordinatorCommands

- (void)showReadingList {
  self.readingListCoordinator = [[ReadingListCoordinator alloc]
      initWithBaseViewController:self.viewController
                    browserState:self.browserState
                          loader:self.viewController];
  [self.readingListCoordinator start];
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

@end

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_coordinator.h"

#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GoogleServicesSettingsCoordinator ()<
    GoogleServicesSettingsViewControllerPresentationDelegate>

// Google services settings mediator.
@property(nonatomic, strong) GoogleServicesSettingsMediator* mediator;

@end

@implementation GoogleServicesSettingsCoordinator

@synthesize delegate = _delegate;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  GoogleServicesSettingsViewController* controller =
      [[GoogleServicesSettingsViewController alloc]
          initWithLayout:layout
                   style:CollectionViewControllerStyleAppBar];
  controller.presentationDelegate = self;
  self.viewController = controller;
  self.mediator = [[GoogleServicesSettingsMediator alloc] init];
  self.mediator.consumer = controller;
  self.mediator.authService =
      AuthenticationServiceFactory::GetForBrowserState(self.browserState);
  controller.modelDelegate = self.mediator;
  DCHECK(self.navigationController);
  [self.navigationController pushViewController:self.viewController
                                       animated:YES];
}

#pragma mark - GoogleServicesSettingsViewControllerPresentationDelegate

- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate googleServicesSettingsCoordinatorDidRemove:self];
}

@end

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_coordinator.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GoogleServicesSettingsCoordinator () <
    GoogleServicesSettingsCommandHandler,
    GoogleServicesSettingsViewControllerPresentationDelegate>

// Google services settings mediator.
@property(nonatomic, strong) GoogleServicesSettingsMediator* mediator;
// Returns the authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Manages the authentication flow for a given identity.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// View controller presented by this coordinator.
@property(nonatomic, strong, readonly)
    GoogleServicesSettingsViewController* googleServicesSettingsViewController;
// The SigninInteractionCoordinator that presents Sign In UI for the
// Settings page.
@property(nonatomic, strong)
    SigninInteractionCoordinator* signinInteractionCoordinator;

@end

@implementation GoogleServicesSettingsCoordinator

- (void)start {
  GoogleServicesSettingsViewController* viewController =
      [[GoogleServicesSettingsViewController alloc]
          initWithTableViewStyle:UITableViewStyleGrouped
                     appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  viewController.presentationDelegate = self;
  self.viewController = viewController;
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(self.browserState);
  self.mediator = [[GoogleServicesSettingsMediator alloc]
      initWithUserPrefService:self.browserState->GetPrefs()
             localPrefService:GetApplicationContext()->GetLocalState()
             syncSetupService:syncSetupService];
  self.mediator.consumer = viewController;
  self.mediator.authService = self.authService;
  self.mediator.identityManager =
      IdentityManagerFactory::GetForBrowserState(self.browserState);
  self.mediator.commandHandler = self;
  self.mediator.syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  viewController.modelDelegate = self.mediator;
  viewController.serviceDelegate = self.mediator;
  DCHECK(self.navigationController);
  [self.navigationController pushViewController:self.viewController
                                       animated:YES];
}

#pragma mark - Private

- (void)authenticationFlowDidComplete {
  DCHECK(self.authenticationFlow);
  self.authenticationFlow = nil;
  [self.googleServicesSettingsViewController allowUserInteraction];
}

- (void)signInInteractionCoordinatorDidComplete {
  self.signinInteractionCoordinator = nil;
}

#pragma mark - Properties

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(self.browserState);
}

- (GoogleServicesSettingsViewController*)googleServicesSettingsViewController {
  return base::mac::ObjCCast<GoogleServicesSettingsViewController>(
      self.viewController);
}

#pragma mark - GoogleServicesSettingsCommandHandler

- (void)restartAuthenticationFlow {
  ChromeIdentity* authenticatedIdentity =
      AuthenticationServiceFactory::GetForBrowserState(self.browserState)
          ->GetAuthenticatedIdentity();
  [self.googleServicesSettingsViewController preventUserInteraction];
  DCHECK(!self.authenticationFlow);
  self.authenticationFlow = [[AuthenticationFlow alloc]
          initWithBrowserState:self.browserState
                      identity:authenticatedIdentity
               shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
              postSignInAction:POST_SIGNIN_ACTION_START_SYNC
      presentingViewController:self.viewController];
  self.authenticationFlow.dispatcher = self.dispatcher;
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    // TODO(crbug.com/889919): Needs to add histogram for |success|.
    [weakSelf authenticationFlowDidComplete];
  }];
}

- (void)openReauthDialogAsSyncIsInAuthError {
  // Sync enters in a permanent auth error state when fetching an access token
  // fails with invalid credentials. This corresponds to Gaia responding with an
  // "invalid grant" error. The current implementation of the iOS SSOAuth
  // library user by Chrome removes the identity from the device when receiving
  // an "invalid grant" response, which leads to the account being also signed
  // out of Chrome. So the sync permanent auth error is a transient state on
  // iOS. The decision was to avoid handling this error in the UI, which means
  // that the reauth dialog is not actually presented on iOS.
}

- (void)openPassphraseDialog {
  [self.dispatcher
      showSyncPassphraseSettingsFromViewController:self.viewController];
}

- (void)showSignIn {
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  DCHECK(!self.signinInteractionCoordinator);
  self.signinInteractionCoordinator = [[SigninInteractionCoordinator alloc]
      initWithBrowserState:self.browserState
                dispatcher:self.dispatcher];
  __weak __typeof(self) weakSelf = self;
  [self.signinInteractionCoordinator
            signInWithIdentity:nil
                   accessPoint:signin_metrics::AccessPoint::
                                   ACCESS_POINT_GOOGLE_SERVICES_SETTINGS
                   promoAction:signin_metrics::PromoAction::
                                   PROMO_ACTION_NO_SIGNIN_PROMO
      presentingViewController:self.navigationController
                    completion:^(BOOL success) {
                      [weakSelf signInInteractionCoordinatorDidComplete];
                    }];
}

#pragma mark - GoogleServicesSettingsViewControllerPresentationDelegate

- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate googleServicesSettingsCoordinatorDidRemove:self];
}

@end

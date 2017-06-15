// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_interaction_controller.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_ui::CompletionCallback;

@interface SigninInteractionController ()<
    ChromeIdentityInteractionManagerDelegate,
    ChromeSigninViewControllerDelegate> {
  ios::ChromeBrowserState* browserState_;
  signin_metrics::AccessPoint accessPoint_;
  signin_metrics::PromoAction promoAction_;
  UIViewController* presentingViewController_;
  BOOL isPresentedOnSettings_;
  BOOL isCancelling_;
  BOOL isDismissing_;
  BOOL interactionManagerDismissalIgnored_;
  AlertCoordinator* alertCoordinator_;
  CompletionCallback completionCallback_;
  ChromeSigninViewController* signinViewController_;
  ChromeIdentityInteractionManager* identityInteractionManager_;
  ChromeIdentity* signInIdentity_;
  BOOL identityAdded_;
}
@end

@implementation SigninInteractionController

- (id)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
            presentingViewController:(UIViewController*)presentingViewController
               isPresentedOnSettings:(BOOL)isPresentedOnSettings
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
                         promoAction:(signin_metrics::PromoAction)promoAction {
  self = [super init];
  if (self) {
    DCHECK(browserState);
    DCHECK(presentingViewController);
    browserState_ = browserState;
    presentingViewController_ = presentingViewController;
    isPresentedOnSettings_ = isPresentedOnSettings;
    accessPoint_ = accessPoint;
    promoAction_ = promoAction;
  }
  return self;
}

- (void)cancel {
  // Cancelling and dismissing the |identityInteractionManager_| may call the
  // |completionCallback_| which could lead to |self| being released before the
  // end of this method. |self| is retained here to prevent this from happening.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
  // Retain this object through the rest of this method in case this object's
  // owner frees this object during the execution of the completion block.
  SigninInteractionController* strongSelf = self;
#pragma clang diagnostic pop
  isCancelling_ = YES;
  [alertCoordinator_ executeCancelHandler];
  [alertCoordinator_ stop];
  [identityInteractionManager_ cancelAndDismissAnimated:NO];
  [signinViewController_ cancel];
  isCancelling_ = NO;
}

- (void)cancelAndDismiss {
  isDismissing_ = YES;
  [self cancel];
  isDismissing_ = NO;
}

- (void)signInWithViewController:(UIViewController*)viewController
                        identity:(ChromeIdentity*)identity
                      completion:(signin_ui::CompletionCallback)completion {
  signin_metrics::LogSigninAccessPointStarted(accessPoint_, promoAction_);
  completionCallback_ = [completion copy];
  ios::ChromeIdentityService* identityService =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  if (identity) {
    DCHECK(identityService->IsValidIdentity(identity));
    DCHECK(!signinViewController_);
    [self showSigninViewControllerWithIdentity:identity identityAdded:NO];
  } else if (identityService->HasIdentities()) {
    DCHECK(!signinViewController_);
    [self showSigninViewControllerWithIdentity:nil identityAdded:NO];
  } else {
    identityInteractionManager_ =
        identityService->NewChromeIdentityInteractionManager(browserState_,
                                                             self);
    if (!identityInteractionManager_) {
      // Abort sign-in if the ChromeIdentityInteractionManager returned is
      // nil (this can happen when the iOS internal provider is not used).
      [self runCompletionCallbackWithSuccess:NO executeCommand:nil];
      return;
    }

    __weak SigninInteractionController* weakSelf = self;
    [identityInteractionManager_
        addAccountWithCompletion:^(ChromeIdentity* identity, NSError* error) {
          [weakSelf handleIdentityAdded:identity
                                  error:error
                           shouldSignIn:YES
                         viewController:viewController];
        }];
  }
}

- (void)reAuthenticateWithCompletion:(CompletionCallback)completion
                      viewController:(UIViewController*)viewController {
  signin_metrics::LogSigninAccessPointStarted(accessPoint_, promoAction_);
  completionCallback_ = [completion copy];
  AccountInfo accountInfo =
      ios::SigninManagerFactory::GetForBrowserState(browserState_)
          ->GetAuthenticatedAccountInfo();
  std::string emailToReauthenticate = accountInfo.email;
  std::string idToReauthenticate = accountInfo.gaia;
  if (emailToReauthenticate.empty() || idToReauthenticate.empty()) {
    // This corresponds to a re-authenticate request after the user was signed
    // out. This corresponds to the case where the identity was removed as a
    // result of the permissions being removed on the server or the identity
    // being removed from another app.
    //
    // Simply use the the last signed-in user email in this case and go though
    // the entire sign-in flow as sync needs to be configured.
    emailToReauthenticate = browserState_->GetPrefs()->GetString(
        prefs::kGoogleServicesLastUsername);
    idToReauthenticate = browserState_->GetPrefs()->GetString(
        prefs::kGoogleServicesLastAccountId);
  }
  DCHECK(!emailToReauthenticate.empty());
  DCHECK(!idToReauthenticate.empty());
  identityInteractionManager_ =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->NewChromeIdentityInteractionManager(browserState_, self);
  __weak SigninInteractionController* weakSelf = self;
  [identityInteractionManager_
      reauthenticateUserWithID:base::SysUTF8ToNSString(idToReauthenticate)
                         email:base::SysUTF8ToNSString(emailToReauthenticate)
                    completion:^(ChromeIdentity* identity, NSError* error) {
                      [weakSelf handleIdentityAdded:identity
                                              error:error
                                       shouldSignIn:YES
                                     viewController:viewController];
                    }];
}

- (void)addAccountWithCompletion:(CompletionCallback)completion
                  viewController:(UIViewController*)viewController {
  completionCallback_ = [completion copy];
  identityInteractionManager_ =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->NewChromeIdentityInteractionManager(browserState_, self);
  __weak SigninInteractionController* weakSelf = self;
  [identityInteractionManager_
      addAccountWithCompletion:^(ChromeIdentity* identity, NSError* error) {
        [weakSelf handleIdentityAdded:identity
                                error:error
                         shouldSignIn:NO
                       viewController:viewController];
      }];
}

#pragma mark - ChromeIdentityInteractionManager operations

- (void)handleIdentityAdded:(ChromeIdentity*)identity
                      error:(NSError*)error
               shouldSignIn:(BOOL)shouldSignIn
             viewController:(UIViewController*)viewController {
  if (!identityInteractionManager_)
    return;

  if (error) {
    // Filter out cancel and errors handled internally by ChromeIdentity.
    if (!ShouldHandleSigninError(error)) {
      [self runCompletionCallbackWithSuccess:NO executeCommand:nil];
      return;
    }

    __weak SigninInteractionController* weakSelf = self;
    ProceduralBlock dismissAction = ^{
      [weakSelf runCompletionCallbackWithSuccess:NO executeCommand:nil];
    };

    alertCoordinator_ = ios_internal::ErrorCoordinator(
        error, dismissAction,
        top_view_controller::TopPresentedViewControllerFrom(viewController));
    [alertCoordinator_ start];
    return;
  }
  if (shouldSignIn) {
    [self showSigninViewControllerWithIdentity:identity identityAdded:YES];
  } else {
    [self runCompletionCallbackWithSuccess:YES executeCommand:nil];
  }
}

- (void)dismissPresentedViewControllersAnimated:(BOOL)animated
                                     completion:(ProceduralBlock)completion {
  if ([presentingViewController_ presentedViewController]) {
    [presentingViewController_ dismissViewControllerAnimated:animated
                                                  completion:completion];
  } else if (completion) {
    completion();
  }
  interactionManagerDismissalIgnored_ = NO;
}

#pragma mark - ChromeIdentityInteractionManagerDelegate

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
     presentViewController:(UIViewController*)viewController
                  animated:(BOOL)animated
                completion:(ProceduralBlock)completion {
  [presentingViewController_ presentViewController:viewController
                                          animated:animated
                                        completion:completion];
}

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
    dismissViewControllerAnimated:(BOOL)animated
                       completion:(ProceduralBlock)completion {
  // Avoid awkward double transitions by not dismissing
  // identityInteractionManager_| if the signin view controller will be
  // displayed on top of it. |identityInteractionManager_| will be dismissed
  // when the signin view controller will be dismissed.
  if ([interactionManager isCanceling]) {
    [self dismissPresentedViewControllersAnimated:animated
                                       completion:completion];
  } else {
    interactionManagerDismissalIgnored_ = YES;
    if (completion) {
      completion();
    }
  }
}

#pragma mark - ChromeSigninViewController operations

- (void)showSigninViewControllerWithIdentity:(ChromeIdentity*)signInIdentity
                               identityAdded:(BOOL)identityAdded {
  signinViewController_ = [[ChromeSigninViewController alloc]
       initWithBrowserState:browserState_
      isPresentedOnSettings:isPresentedOnSettings_
                accessPoint:accessPoint_
                promoAction:promoAction_
             signInIdentity:signInIdentity];
  [signinViewController_ setDelegate:self];
  [signinViewController_
      setModalPresentationStyle:UIModalPresentationFormSheet];
  [signinViewController_
      setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  signInIdentity_ = signInIdentity;
  identityAdded_ = identityAdded;

  UIViewController* presentingViewController = presentingViewController_;
  if (identityInteractionManager_) {
    // If |identityInteractionManager_| is currently displayed,
    // |signinViewController_| is presented on top of it (instead of on top of
    // |presentingViewController_|), to avoid an awkward transition (dismissing
    // |identityInteractionManager_|, followed by presenting
    // |signinViewController_|).
    while (presentingViewController.presentedViewController) {
      presentingViewController =
          presentingViewController.presentedViewController;
    }
  }
  [presentingViewController presentViewController:signinViewController_
                                         animated:YES
                                       completion:nil];
}

- (void)dismissSigninViewControllerWithSignInSuccess:(BOOL)success
                                      executeCommand:
                                          (GenericChromeCommand*)command {
  DCHECK(signinViewController_);
  if ((isCancelling_ && !isDismissing_) ||
      ![presentingViewController_ presentedViewController]) {
    [self runCompletionCallbackWithSuccess:success executeCommand:command];
    return;
  }
  ProceduralBlock completion = ^{
    [self runCompletionCallbackWithSuccess:success executeCommand:command];
  };
  [self dismissPresentedViewControllersAnimated:YES completion:completion];
}

#pragma mark - ChromeSigninViewControllerDelegate

- (void)willStartSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
}

- (void)willStartAddAccount:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
}

- (void)didSkipSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
  [self dismissSigninViewControllerWithSignInSuccess:NO executeCommand:nil];
}

- (void)didSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
}

- (void)didUndoSignIn:(ChromeSigninViewController*)controller
             identity:(ChromeIdentity*)identity {
  DCHECK_EQ(controller, signinViewController_);
  if ([signInIdentity_ isEqual:identity]) {
    signInIdentity_ = nil;
    if (identityAdded_) {
      // This is best effort. If the operation fails, the account will be left
      // on the device. The user will not be warned either as this call is
      // asynchronous (but undo is not), the application might be in an unknown
      // state when the forget identity operation finishes.
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->ForgetIdentity(identity, nil);
    }
    [self dismissSigninViewControllerWithSignInSuccess:NO executeCommand:nil];
  }
}

- (void)didFailSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
  [self dismissSigninViewControllerWithSignInSuccess:NO executeCommand:nil];
}

- (void)didAcceptSignIn:(ChromeSigninViewController*)controller
         executeCommand:(GenericChromeCommand*)command {
  DCHECK_EQ(controller, signinViewController_);
  [self dismissSigninViewControllerWithSignInSuccess:YES
                                      executeCommand:command];
}

#pragma mark - Utility methods

- (void)runCompletionCallbackWithSuccess:(BOOL)success
                          executeCommand:(GenericChromeCommand*)command {
  // In order to avoid awkward double transitions, |identityInteractionManager_|
  // is not dismissed when requested (except when canceling). However, in case
  // of errors, |identityInteractionManager_| needs to be directly dismissed,
  // which is done here.
  if (interactionManagerDismissalIgnored_) {
    [self dismissPresentedViewControllersAnimated:YES completion:nil];
  }

  identityInteractionManager_ = nil;
  signinViewController_ = nil;
  UIViewController* presentingViewController = presentingViewController_;
  // Ensure self is not destroyed in the callbacks.
  SigninInteractionController* strongSelf = self;
  if (completionCallback_) {
    completionCallback_(success);
    completionCallback_ = nil;
  }
  strongSelf = nil;
  if (command) {
    [presentingViewController chromeExecuteCommand:command];
  }
}

@end

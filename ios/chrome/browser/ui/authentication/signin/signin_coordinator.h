// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class Browser;
@class ChromeIdentity;
namespace syncer {
enum class KeyRetrievalTriggerForUMA;
}  // namespace syncer

// Main class for sign-in coordinator. This class should not be instantiated
// directly, this should be done using the class methods.
@interface SigninCoordinator : ChromeCoordinator

// Called when the sign-in dialog is interrupted, canceled or successful.
// This completion needs to be set before calling -[SigninCoordinator start].
@property(nonatomic, copy) SigninCoordinatorCompletionCallback signinCompletion;

// Returns YES if the Google services settings view is presented.
// TODO(crbug.com/971989): This property exists for the implementation
// transition.
@property(nonatomic, assign, readonly, getter=isSettingsViewPresented)
    BOOL settingsViewPresented;

// Returns a coordinator for user sign-in workflow.
// |viewController| presents the sign-in.
// |identity| is the identity preselected with the sign-in opens.
// |accessPoint| is the view where the sign-in button was displayed.
// |promoAction| is promo button used to trigger the sign-in.
+ (instancetype)
    userSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                       identity:(ChromeIdentity*)identity
                                    accessPoint:
                                        (signin_metrics::AccessPoint)accessPoint
                                    promoAction:(signin_metrics::PromoAction)
                                                    promoAction;

// Returns a coordinator for first run sign-in workflow. If the user tap on the
// settings link to open the advanced settings sign-in, the SigninCoordinator
// owner is in charge open this view, according to -[SigninCompletionInfo
// signinCompletionAction] in |signinCompletionInfo| from |signinCompletion|.
// |navigationController| presents the sign-in. Will be responsible for
// dismissing itself upon sign-in completion.
+ (instancetype)firstRunCoordinatorWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                                        browser:
                                                            (Browser*)browser;

// Returns a coordinator for upgrade sign-in workflow.
// |viewController| presents the sign-in.
+ (instancetype)upgradeSigninPromoCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                            browser:(Browser*)
                                                                        browser;

// Returns a coordinator for advanced sign-in settings workflow.
// |viewController| presents the sign-in.
+ (instancetype)
    advancedSettingsSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser;

// Returns a coordinator to add an account.
// |viewController| presents the sign-in.
// |accessPoint| access point from the sign-in where is started.
+ (instancetype)
    addAccountCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                    accessPoint:(signin_metrics::AccessPoint)
                                                    accessPoint;

// Returns a coordinator for re-authentication workflow.
// |viewController| presents the sign-in.
// |accessPoint| access point from the sign-in where is started.
// |promoAction| is promo button used to trigger the sign-in.
+ (instancetype)
    reAuthenticationCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                              browser:(Browser*)browser
                                          accessPoint:
                                              (signin_metrics::AccessPoint)
                                                  accessPoint
                                          promoAction:
                                              (signin_metrics::PromoAction)
                                                  promoAction;

// Returns a coordinator for re-authentication workflow for Trusted
// Vault for the primary identity. This is done with ChromeTrustedVaultService.
// Related to IOSTrustedVaultClient.
// |viewController| presents the sign-in.
// |retrievalTrigger| UI elements where the trusted vault reauth has been
// triggered.
+ (instancetype)
    trustedVaultReAuthenticationCoordiantorWithBaseViewController:
        (UIViewController*)viewController
                                                          browser:
                                                              (Browser*)browser
                                                 retrievalTrigger:
                                                     (syncer::
                                                          KeyRetrievalTriggerForUMA)
                                                         retrievalTrigger;

// Interrupts the sign-in flow.
// |signinCompletion(SigninCoordinatorResultInterrupted, nil)| is guaranteed to
// be called before |completion()|.
// |action| action describing how to interrupt the sign-in.
// |completion| called once the sign-in is fully interrupted.
- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion;

// ChromeCoordinator.
- (void)start NS_REQUIRES_SUPER;
- (void)stop NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_

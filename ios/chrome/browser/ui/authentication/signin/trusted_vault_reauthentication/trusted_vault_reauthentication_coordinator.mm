// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/trusted_vault_reauthentication/trusted_vault_reauthentication_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/driver/sync_service_utils.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_trusted_vault_service.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysNSStringToUTF16;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

@interface TrustedVaultReauthenticationCoordinator ()

@property(nonatomic, strong) AlertCoordinator* errorAlertCoordinator;
@property(nonatomic, strong) ChromeIdentity* identity;

@end

@implementation TrustedVaultReauthenticationCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          retrievalTrigger:(syncer::KeyRetrievalTriggerForUMA)
                                               retrievalTrigger {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    syncer::RecordKeyRetrievalTrigger(retrievalTrigger);
  }
  return self;
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  ios::ChromeBrowserProvider* browserProvider = ios::GetChromeBrowserProvider();
  ios::ChromeTrustedVaultService* trustedVaultService =
      browserProvider->GetChromeTrustedVaultService();
  BOOL animated;
  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss:
      // Not supported by Trusted Vault UI, replaced by dismiss without
      // animation.
    case SigninCoordinatorInterruptActionDismissWithoutAnimation:
      animated = NO;
      break;
    case SigninCoordinatorInterruptActionDismissWithAnimation:
      animated = YES;
      break;
  }
  __weak __typeof(self) weakSelf = self;
  void (^cancelCompletion)(void) = ^() {
    // The reauthentication callback is dropped when the dialog is canceled.
    // The completion block has to be called explicitly.
    [weakSelf
        runCompletionCallbackWithSigninResult:SigninCoordinatorResultInterrupted
                                     identity:self.identity
                   showAdvancedSettingsSignin:NO];
    if (completion) {
      completion();
    }
  };
  trustedVaultService->CancelReauthentication(animated, cancelCompletion);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  // TODO(crbug.com/1019685): keys should be fetched to be sure the reauth is
  // still needed. The reauth should be really started only when fetch is
  // failed. If the fetch is success full, the coordinator can be closed
  // successfuly.
  ios::ChromeBrowserProvider* browserProvider = ios::GetChromeBrowserProvider();
  ios::ChromeTrustedVaultService* trustedVaultService =
      browserProvider->GetChromeTrustedVaultService();
  self.identity = AuthenticationServiceFactory::GetForBrowserState(
                      self.browser->GetBrowserState())
                      ->GetAuthenticatedIdentity();
  __weak __typeof(self) weakSelf = self;
  void (^callback)(BOOL success, NSError* error) =
      ^(BOOL success, NSError* error) {
        if (error) {
          [weakSelf displayError:error];
        } else {
          [weakSelf reauthentificationCompletedWithSuccess:success];
        }
      };
  trustedVaultService->Reauthentication(self.identity, self.baseViewController,
                                        callback);
}

#pragma mark - Private

- (void)displayError:(NSError*)error {
  DCHECK(error);
  NSString* title = GetNSString(IDS_IOS_SIGN_TRUSTED_VAULT_ERROR_DIALOG_TITLE);
  NSString* errorCode = [NSString stringWithFormat:@"%ld", error.code];
  NSString* message =
      GetNSStringF(IDS_IOS_SIGN_TRUSTED_VAULT_ERROR_DIALOG_MESSAGE,
                   SysNSStringToUTF16(errorCode));
  self.errorAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];
  __weak __typeof(self) weakSelf = self;
  [self.errorAlertCoordinator
      addItemWithTitle:GetNSString(IDS_OK)
                action:^{
                  [weakSelf reauthentificationCompletedWithSuccess:NO];
                }
                 style:UIAlertActionStyleDefault];
  [self.errorAlertCoordinator start];
}

- (void)reauthentificationCompletedWithSuccess:(BOOL)success {
  DCHECK(self.identity);
  SigninCoordinatorResult result = success
                                       ? SigninCoordinatorResultSuccess
                                       : SigninCoordinatorResultCanceledByUser;
  [self runCompletionCallbackWithSigninResult:result
                                     identity:self.identity
                   showAdvancedSettingsSignin:NO];
}

@end

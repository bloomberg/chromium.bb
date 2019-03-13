// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_password_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_view_controller.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarPasswordCoordinator () <InfobarBannerDelegate,
                                          InfobarModalDelegate>

// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly)
    IOSChromePasswordManagerInfoBarDelegate* passwordInfoBarDelegate;
// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// InfobarModalViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarModalViewController* modalViewController;

@end

@implementation InfobarPasswordCoordinator
// Property defined in InfobarCoordinating.
@synthesize bannerViewController = _bannerViewController;
// Property defined in InfobarCoordinating.
@synthesize badgeDelegate = _badgeDelegate;
// Property defined in InfobarCoordinating.
@synthesize bannerTransitionDriver = _bannerTransitionDriver;
// Property defined in InfobarUIDelegate.
@synthesize delegate = _delegate;
// Property defined in InfobarCoordinating.
@synthesize modalTransitionDriver = _modalTransitionDriver;
// Property defined in InfobarUIDelegate.
@synthesize presented = _presented;
// Property defined in InfobarCoordinating.
@synthesize started = _started;

- (instancetype)initWithInfoBarDelegate:
    (IOSChromePasswordManagerInfoBarDelegate*)passwordInfoBarDelegate {
  self = [super initWithBaseViewController:nil browserState:nil];
  if (self) {
    _passwordInfoBarDelegate = passwordInfoBarDelegate;
    _presented = YES;
  }
  return self;
}

- (void)start {
  self.started = YES;
  self.bannerViewController =
      [[InfobarBannerViewController alloc] initWithDelegate:self];
  self.bannerViewController.titleText =
      base::SysUTF16ToNSString(self.passwordInfoBarDelegate->GetMessageText());
  self.bannerViewController.buttonText =
      base::SysUTF16ToNSString(self.passwordInfoBarDelegate->GetButtonLabel(
          ConfirmInfoBarDelegate::BUTTON_OK));
  self.bannerViewController.iconImage =
      [UIImage imageNamed:@"infobar_passwords_icon"];
}

- (void)stop {
  if (self.started) {
    self.started = NO;
    // RemoveInfoBar() will delete the InfobarIOS that owns this Coordinator
    // from memory.
    self.delegate->RemoveInfoBar();
  }
}

#pragma mark - InfobarUIDelegate

- (void)removeView {
  [self dismissInfobarBanner:self.bannerViewController];
}

- (void)detachView {
  [self dismissInfobarBanner:self.bannerViewController];
  [self stop];
}

#pragma mark - InfobarCoordinating

- (void)presentInfobarModalFrom:(UIViewController*)baseViewController {
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBase];
  [self presentInfobarModalFrom:baseViewController
                         driver:self.modalTransitionDriver];
}

- (void)presentInfobarBannerFrom:(UIViewController*)baseViewController {
  [self.bannerViewController
      setModalPresentationStyle:UIModalPresentationCustom];
  self.bannerTransitionDriver = [[InfobarBannerTransitionDriver alloc] init];
  self.bannerViewController.transitioningDelegate = self.bannerTransitionDriver;
  [baseViewController presentViewController:self.bannerViewController
                                   animated:YES
                                 completion:nil];
}

#pragma mark - InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  self.passwordInfoBarDelegate->Accept();
  [self.badgeDelegate infobarWasAccepted];
  [self dismissInfobarBanner:self.bannerViewController];
}

- (void)dismissInfobarBanner:(id)sender {
  [self.bannerViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           self.bannerTransitionDriver = nil;
                         }];
}

- (void)presentInfobarModalFromBanner {
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  [self presentInfobarModalFrom:self.bannerViewController
                         driver:self.modalTransitionDriver];
}

#pragma mark - InfobarModalDelegate

- (void)dismissInfobarModal:(UIButton*)sender {
  if (self.modalTransitionDriver.transitionMode ==
      InfobarModalTransitionBanner) {
    [self.bannerViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [self.badgeDelegate infobarModalWasDismissed];
                             // Since the Modal was presented by the
                             // BannerViewController, dismiss that too.
                             [self dismissInfobarBanner:
                                       self.bannerViewController];
                             self.modalTransitionDriver = nil;
                           }];
  } else {
    [self.modalViewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [self.badgeDelegate infobarModalWasDismissed];
                             self.modalTransitionDriver = nil;
                           }];
  }
}

#pragma mark - Private

- (void)presentInfobarModalFrom:(UIViewController*)presentingViewController
                         driver:(InfobarModalTransitionDriver*)driver {
  self.modalViewController =
      [[InfobarModalViewController alloc] initWithModalDelegate:self];
  self.modalViewController.title =
      base::SysUTF16ToNSString(self.passwordInfoBarDelegate->GetMessageText());

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = driver;
  navController.modalPresentationStyle = UIModalPresentationCustom;
  [presentingViewController presentViewController:navController
                                         animated:YES
                                       completion:nil];
  [self.badgeDelegate infobarModalWasPresented];
}

@end

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_password_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_view_controller.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_expand_banner_animator.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_presentation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarPasswordCoordinator () <InfobarBannerDelegate,
                                          InfobarModalDelegate,
                                          UIViewControllerTransitioningDelegate>

// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly)
    IOSChromePasswordManagerInfoBarDelegate* passwordInfoBarDelegate;
// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;

@end

@implementation InfobarPasswordCoordinator
// Property defined in InfobarCoordinating.
@synthesize bannerViewController = _bannerViewController;
// Property defined in InfobarUIDelegate.
@synthesize delegate = _delegate;
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
    [self.bannerViewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
}

#pragma mark - InfobarUIDelegate

- (void)removeView {
  [self stop];
}

- (void)detachView {
  [self stop];
  // RemoveInfoBar() will delete the InfobarIOS that owns this Coordinator
  // from memory.
  self.delegate->RemoveInfoBar();
}

#pragma mark - InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  self.passwordInfoBarDelegate->Accept();
  [self dismissInfobarBanner:self.bannerViewController];
}

- (void)dismissInfobarBanner:(id)sender {
  [self stop];
}

- (void)presentInfobarModal {
  InfobarModalViewController* expandedViewController =
      [[InfobarModalViewController alloc] initWithModalDelegate:self];
  expandedViewController.transitioningDelegate = self;
  [expandedViewController setModalPresentationStyle:UIModalPresentationCustom];
  [self.bannerViewController presentViewController:expandedViewController
                                          animated:YES
                                        completion:nil];
}

#pragma mark - InfobarModalDelegate

- (void)dismissInfobarModal:(UIViewController*)sender {
  [self.bannerViewController dismissViewControllerAnimated:YES
                                                completion:^{
                                                  [self stop];
                                                }];
}

// TODO(crbug.com/911864): Create a Transitioning object that can be shared
// with all Infobar Coordinators.
#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  InfobarModalPresentationController* presentationController =
      [[InfobarModalPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  InfobarExpandBannerAnimator* animator =
      [[InfobarExpandBannerAnimator alloc] init];
  animator.presenting = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  InfobarExpandBannerAnimator* animator =
      [[InfobarExpandBannerAnimator alloc] init];
  animator.presenting = NO;
  return animator;
}

@end

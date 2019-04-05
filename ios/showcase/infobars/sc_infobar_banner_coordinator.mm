// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/infobars/sc_infobar_banner_coordinator.h"

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_view_controller.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kInfobarBannerTitleLabel = @"Test Infobar";
NSString* const kInfobarBannerSubtitleLabel = @"This a test Infobar.";
NSString* const kInfobarBannerButtonLabel = @"Accept";
NSString* const kInfobarBannerPresentedModalLabel = @"Modal Infobar";

#pragma mark - ContainerViewController

@interface ContainerViewController : UIViewController <InfobarBannerPositioner>
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
@property(nonatomic, strong)
    InfobarBannerTransitionDriver* bannerTransitionDriver;
@end

@implementation ContainerViewController
- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.bannerViewController
      setModalPresentationStyle:UIModalPresentationCustom];
  self.bannerTransitionDriver = [[InfobarBannerTransitionDriver alloc] init];
  self.bannerTransitionDriver.bannerPositioner = self;
  self.bannerViewController.transitioningDelegate = self.bannerTransitionDriver;
  [self presentViewController:self.bannerViewController
                     animated:YES
                   completion:nil];
}

#pragma mark InfobarBannerPositioner

- (CGFloat)bannerYPosition {
  return 100;
}

- (UIView*)bannerView {
  return self.bannerViewController.view;
}
@end

#pragma mark - SCInfobarBannerCoordinator

@interface SCInfobarBannerCoordinator () <InfobarBannerDelegate,
                                          InfobarModalDelegate>
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
@property(nonatomic, strong) ContainerViewController* containerViewController;
@property(nonatomic, strong)
    InfobarModalTransitionDriver* modalTransitionDriver;
@property(nonatomic, strong) InfobarModalViewController* modalViewController;
@end

@implementation SCInfobarBannerCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  self.containerViewController = [[ContainerViewController alloc] init];
  UIView* containerView = self.containerViewController.view;
  containerView.backgroundColor = [UIColor whiteColor];
  self.containerViewController.title = @"Infobar Messages";

  self.bannerViewController =
      [[InfobarBannerViewController alloc] initWithDelegate:self];
  self.bannerViewController.titleText = kInfobarBannerTitleLabel;
  self.bannerViewController.subTitleText = kInfobarBannerSubtitleLabel;
  self.bannerViewController.buttonText = kInfobarBannerButtonLabel;
  self.containerViewController.bannerViewController = self.bannerViewController;

  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

- (void)dealloc {
  [self dismissInfobarBanner:nil animated:YES completion:nil];
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  [self dismissInfobarBanner:nil animated:YES completion:nil];
}

- (void)presentInfobarModalFromBanner {
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  self.modalViewController =
      [[InfobarModalViewController alloc] initWithModalDelegate:self];
  self.modalViewController.title = kInfobarBannerPresentedModalLabel;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = self.modalTransitionDriver;
  navController.modalPresentationStyle = UIModalPresentationCustom;

  [self.bannerViewController presentViewController:navController
                                          animated:YES
                                        completion:nil];
}

- (void)dismissInfobarBanner:(id)sender
                    animated:(BOOL)animated
                  completion:(ProceduralBlock)completion {
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:nil];
}

#pragma mark InfobarModalDelegate

- (void)modalInfobarButtonWasPressed:(UIButton*)sender {
  [self dismissInfobarModal:sender completion:nil];
}

- (void)dismissInfobarModal:(UIButton*)sender
                 completion:(ProceduralBlock)completion {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

@end

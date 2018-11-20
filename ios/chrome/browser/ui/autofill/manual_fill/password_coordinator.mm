// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_coordinator.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_animator.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const PasswordDoneButtonAccessibilityIdentifier =
    @"kManualFillPasswordDoneButtonAccessibilityIdentifier";

}  // namespace manual_fill

@interface ManualFillPasswordCoordinator ()<
    PasswordListDelegate,
    UIViewControllerTransitioningDelegate>

// Fetches and filters the passwords for the view controller.
@property(nonatomic, strong) ManualFillPasswordMediator* passwordMediator;

// The view controller presented above the keyboard where the user can select
// one of their passwords.
@property(nonatomic, strong) PasswordViewController* passwordViewController;

// The view controller modally presented where the user can select one of their
// passwords. Owned by the view controllers hierarchy.
@property(nonatomic, weak) PasswordViewController* allPasswordsViewController;

// Button presenting this coordinator in a popover. Used for continuation after
// dismissing any presented view controller. iPad only.
@property(nonatomic, weak) UIButton* presentingButton;

@end

@implementation ManualFillPasswordCoordinator

// Property tagged dynamic because it overrides super class delegate with and
// extension of the super delegate type (i.e. PasswordCoordinatorDelegate
// extends FallbackCoordinatorDelegate)
@dynamic delegate;

- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
              webStateList:(WebStateList*)webStateList
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState
                          injectionHandler:injectionHandler];
  if (self) {
    _passwordViewController =
        [[PasswordViewController alloc] initWithSearchController:nil];
    _passwordViewController.contentInsetsAlwaysEqualToSafeArea = YES;

    auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState, ServiceAccessType::EXPLICIT_ACCESS);
    _passwordMediator =
        [[ManualFillPasswordMediator alloc] initWithWebStateList:webStateList
                                                   passwordStore:passwordStore];
    _passwordMediator.consumer = _passwordViewController;
    _passwordMediator.navigationDelegate = self;
    _passwordMediator.contentDelegate = injectionHandler;
  }
  return self;
}

- (void)stop {
  [super stop];
  [self.allPasswordsViewController dismissViewControllerAnimated:YES
                                                      completion:nil];
}

- (void)presentFromButton:(UIButton*)button {
  [super presentFromButton:button];
  self.presentingButton = button;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  TableViewPresentationController* presentationController =
      [[TableViewPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  UITraitCollection* traitCollection = presenting.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  UITraitCollection* traitCollection = dismissed.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = NO;
  animator.direction = TableAnimatorDirectionFromLeading;
  return animator;
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.passwordViewController;
}

#pragma mark - PasswordListDelegate

- (void)openAllPasswordsList {
  // On iPad, first dismiss the popover before the new view is presented.
  __weak __typeof(self) weakSelf = self;
  if (IsIPadIdiom() && self.passwordViewController.presentingViewController) {
    [self.passwordViewController
        dismissViewControllerAnimated:true
                           completion:^{
                             [weakSelf openAllPasswordsList];
                           }];
    return;
  }
  UISearchController* searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  searchController.searchResultsUpdater = self.passwordMediator;

  PasswordViewController* allPasswordsViewController = [
      [PasswordViewController alloc] initWithSearchController:searchController];
  self.passwordMediator.disableFilter = YES;
  self.passwordMediator.consumer = allPasswordsViewController;
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissPresentedViewController)];
  doneButton.accessibilityIdentifier =
      manual_fill::PasswordDoneButtonAccessibilityIdentifier;
  allPasswordsViewController.navigationItem.rightBarButtonItem = doneButton;
  self.allPasswordsViewController = allPasswordsViewController;

  TableViewNavigationController* navigationController =
      [[TableViewNavigationController alloc]
          initWithTable:allPasswordsViewController];
  navigationController.transitioningDelegate = self;
  [navigationController setModalPresentationStyle:UIModalPresentationCustom];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)dismissPresentedViewController {
  // Dismiss the full screen view controller and present the pop over.
  __weak __typeof(self) weakSelf = self;
  [self.allPasswordsViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           if (weakSelf.presentingButton) {
                             [weakSelf
                                 presentFromButton:weakSelf.presentingButton];
                           }
                         }];
}

- (void)openPasswordSettings {
  __weak id<PasswordCoordinatorDelegate> delegate = self.delegate;
  [self dismissIfNecessaryThenDoCompletion:^{
    [delegate openPasswordSettings];
  }];
}

@end

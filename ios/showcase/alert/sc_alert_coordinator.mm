// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/alert/sc_alert_coordinator.h"

#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCAlertCoordinator ()
@property(nonatomic, strong) UIViewController* containerViewController;
@end

@implementation SCAlertCoordinator

@synthesize baseViewController = _baseViewController;

- (void)start {
  self.containerViewController = [[UIViewController alloc] init];
  self.containerViewController.definesPresentationContext = YES;
  self.containerViewController.title = @"Alert";

  UIView* containerView = self.containerViewController.view;
  containerView.backgroundColor = [UIColor whiteColor];

  UIButton* alertButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [alertButton setTitle:@"alert()" forState:UIControlStateNormal];
  [alertButton addTarget:self
                  action:@selector(showAlert)
        forControlEvents:UIControlEventTouchUpInside];

  UIButton* promptButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [promptButton setTitle:@"prompt()" forState:UIControlStateNormal];
  [promptButton addTarget:self
                   action:@selector(showPrompt)
         forControlEvents:UIControlEventTouchUpInside];

  UIButton* confirmButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [confirmButton setTitle:@"confirm()" forState:UIControlStateNormal];
  [confirmButton addTarget:self
                    action:@selector(showConfirm)
          forControlEvents:UIControlEventTouchUpInside];

  UIButton* authButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [authButton setTitle:@"HTTP Auth" forState:UIControlStateNormal];
  [authButton addTarget:self
                 action:@selector(showHTTPAuth)
       forControlEvents:UIControlEventTouchUpInside];

  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
    alertButton, promptButton, confirmButton, authButton
  ]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = 30;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [containerView addSubview:stack];

  [NSLayoutConstraint activateConstraints:@[
    [stack.centerXAnchor constraintEqualToAnchor:containerView.centerXAnchor],
    [stack.centerYAnchor constraintEqualToAnchor:containerView.centerYAnchor],
  ]];
  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

- (void)showAlert {
  AlertViewController* alert = [[AlertViewController alloc] init];
  alert.title = @"chromium.org says";
  alert.message = @"This is an alert message from a website.";
  __weak __typeof(self) weakSelf = self;
  AlertAction* action =
      [AlertAction actionWithTitle:@"OK"
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:action];
  [self presentAlertViewController:alert];
}

- (void)showPrompt {
  AlertViewController* alert = [[AlertViewController alloc] init];
  alert.title = @"chromium.org says";
  alert.message = @"This is a promp message from a website.";
  [alert addTextFieldWithConfigurationHandler:^(UITextField* textField) {
    textField.placeholder = @"placehorder";
  }];
  __weak __typeof(self) weakSelf = self;
  AlertAction* OKAction =
      [AlertAction actionWithTitle:@"OK"
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:OKAction];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:cancelAction];
  [self presentAlertViewController:alert];
}

- (void)showConfirm {
  AlertViewController* alert = [[AlertViewController alloc] init];
  alert.title = @"chromium.org says";
  alert.message = @"This is a confirm message from a website.";
  __weak __typeof(self) weakSelf = self;
  AlertAction* OKAction =
      [AlertAction actionWithTitle:@"OK"
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:OKAction];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:cancelAction];
  [self presentAlertViewController:alert];
}

- (void)showHTTPAuth {
  AlertViewController* alert = [[AlertViewController alloc] init];
  alert.title = @"Sign in";
  alert.message =
      @"https://www.chromium.org requires a username and a password.";
  [alert addTextFieldWithConfigurationHandler:^(UITextField* textField) {
    textField.placeholder = @"Username";
  }];
  [alert addTextFieldWithConfigurationHandler:^(UITextField* textField) {
    textField.placeholder = @"Password";
    textField.secureTextEntry = YES;
  }];
  __weak __typeof(self) weakSelf = self;
  AlertAction* OKAction =
      [AlertAction actionWithTitle:@"Sign In"
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:OKAction];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:cancelAction];
  [self presentAlertViewController:alert];
}

- (void)presentAlertViewController:(AlertViewController*)alertViewController {
  alertViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  alertViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  [self.containerViewController presentViewController:alertViewController
                                             animated:true
                                           completion:nil];
}

@end

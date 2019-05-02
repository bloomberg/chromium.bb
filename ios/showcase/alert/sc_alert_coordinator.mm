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
@property(nonatomic, strong) UISwitch* blockAlertSwitch;
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

  UIButton* longButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [longButton setTitle:@"Long Alert" forState:UIControlStateNormal];
  [longButton addTarget:self
                 action:@selector(showLongAlert)
       forControlEvents:UIControlEventTouchUpInside];

  UILabel* blockAlertsLabel = [[UILabel alloc] init];
  blockAlertsLabel.text = @"Show \"Block Alerts Button\"";

  self.blockAlertSwitch = [[UISwitch alloc] init];

  UIStackView* switchStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.blockAlertSwitch, blockAlertsLabel ]];
  switchStack.axis = UILayoutConstraintAxisHorizontal;
  switchStack.spacing = 16;
  switchStack.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    alertButton, promptButton, confirmButton, authButton, longButton,
    switchStack
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = 30;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStack.distribution = UIStackViewDistributionFillEqually;
  [containerView addSubview:verticalStack];

  [NSLayoutConstraint activateConstraints:@[
    [verticalStack.centerXAnchor
        constraintEqualToAnchor:containerView.centerXAnchor],
    [verticalStack.centerYAnchor
        constraintEqualToAnchor:containerView.centerYAnchor],
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
                             style:UIAlertActionStyleDefault
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
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:OKAction];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                             style:UIAlertActionStyleCancel
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
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:OKAction];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                             style:UIAlertActionStyleCancel
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
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:OKAction];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                             style:UIAlertActionStyleCancel
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:cancelAction];
  [self presentAlertViewController:alert];
}

- (void)showLongAlert {
  AlertViewController* alert = [[AlertViewController alloc] init];
  alert.title = @"Sign in";
  alert.message =
      @"It was the best of times, it was the worst of times, it was the age of "
      @"wisdom, it was the age of foolishness, it was the epoch of belief, it "
      @"was the epoch of incredulity, it was the season of Light, it was the "
      @"season of Darkness, it was the spring of hope, it was the winter of "
      @"despair, we had everything before us, we had nothing before us, we "
      @"were all going direct to Heaven, we were all going direct the other "
      @"way.";
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
                             style:UIAlertActionStyleDefault
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  AlertAction* cancelAction =
      [AlertAction actionWithTitle:@"Cancel"
                             style:UIAlertActionStyleCancel
                           handler:^(AlertAction* action) {
                             [weakSelf.containerViewController
                                 dismissViewControllerAnimated:YES
                                                    completion:nil];
                           }];
  [alert addAction:OKAction];
  [alert addAction:cancelAction];
  [self presentAlertViewController:alert];
}

- (void)presentAlertViewController:(AlertViewController*)alertViewController {
  if (self.blockAlertSwitch.isOn) {
    __weak __typeof(self) weakSelf = self;
    AlertAction* blockAction =
        [AlertAction actionWithTitle:@"Block Dialogs"
                               style:UIAlertActionStyleDestructive
                             handler:^(AlertAction* action) {
                               [weakSelf.containerViewController
                                   dismissViewControllerAnimated:YES
                                                      completion:nil];
                             }];
    [alertViewController addAction:blockAction];
  }
  alertViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  alertViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  [self.containerViewController presentViewController:alertViewController
                                             animated:true
                                           completion:nil];
}

@end

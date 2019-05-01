// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_password_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarPasswordCoordinator () <InfobarCoordinatorImplementation,
                                          InfobarPasswordModalDelegate>

// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly)
    IOSChromeSavePasswordInfoBarDelegate* passwordInfoBarDelegate;
// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// InfobarPasswordTableViewController owned by this Coordinator.
@property(nonatomic, strong)
    InfobarPasswordTableViewController* modalViewController;

@end

@implementation InfobarPasswordCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize modalViewController = _modalViewController;

- (instancetype)initWithInfoBarDelegate:
    (IOSChromeSavePasswordInfoBarDelegate*)passwordInfoBarDelegate {
  self = [super initWithInfoBarDelegate:passwordInfoBarDelegate];
  if (self) {
    _passwordInfoBarDelegate = passwordInfoBarDelegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.started = YES;
  self.bannerViewController =
      [[InfobarBannerViewController alloc] initWithDelegate:self];
  self.bannerViewController.titleText =
      base::SysUTF16ToNSString(self.passwordInfoBarDelegate->GetMessageText());
  NSString* username = self.passwordInfoBarDelegate->GetUserNameText();
  NSString* password = self.passwordInfoBarDelegate->GetPasswordText();
  password = [@"" stringByPaddingToLength:[password length]
                               withString:@"•"
                          startingAtIndex:0];
  self.bannerViewController.subTitleText =
      [NSString stringWithFormat:@"%@ %@", username, password];
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

#pragma mark - InfobarCoordinatorImplementation

- (void)configureModalViewController {
  self.modalViewController = [[InfobarPasswordTableViewController alloc]
      initWithTableViewStyle:UITableViewStylePlain
                 appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  self.modalViewController.infobarModalDelegate = self;
  self.modalViewController.title =
      self.passwordInfoBarDelegate->GetInfobarModalTitleText();
  self.modalViewController.username =
      self.passwordInfoBarDelegate->GetUserNameText();
  NSString* password = self.passwordInfoBarDelegate->GetPasswordText();
  self.modalViewController.maskedPassword =
      [@"" stringByPaddingToLength:[password length]
                        withString:@"•"
                   startingAtIndex:0];
  self.modalViewController.unmaskedPassword = password;
  self.modalViewController.detailsTextMessage =
      self.passwordInfoBarDelegate->GetDetailsMessageText();
  self.modalViewController.saveButtonText =
      base::SysUTF16ToNSString(self.passwordInfoBarDelegate->GetButtonLabel(
          ConfirmInfoBarDelegate::BUTTON_OK));
  self.modalViewController.cancelButtonText =
      base::SysUTF16ToNSString(self.passwordInfoBarDelegate->GetButtonLabel(
          ConfirmInfoBarDelegate::BUTTON_CANCEL));
  self.modalViewController.URL = self.passwordInfoBarDelegate->GetURLHostText();
}

- (void)dismissBannerWhenInteractionIsFinished {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (void)performInfobarAction {
  self.passwordInfoBarDelegate->Accept();
}

- (void)infobarWasDismissed {
  // Release these strong ViewControllers at the time of infobar dismissal.
  self.bannerViewController = nil;
  self.modalViewController = nil;
}

- (CGFloat)infobarModalContentHeight {
  UITableView* tableView = self.modalViewController.tableView;
  [tableView setNeedsLayout];
  [tableView layoutIfNeeded];
  return tableView.contentSize.height;
}

#pragma mark - InfobarPasswordModalDelegate

- (void)updateCredentialsWithUsername:(NSString*)username
                             password:(NSString*)password {
  self.passwordInfoBarDelegate->UpdateCredentials(username, password);
  [self modalInfobarButtonWasAccepted:self];
}

- (void)neverSaveCredentialsForCurrentSite {
  self.passwordInfoBarDelegate->Cancel();
  // Completely remove the Infobar along with its badge after blacklisting the
  // Website.
  [self detachView];
}

- (void)presentPasswordSettings {
  DCHECK(self.dispatcher);
  [self
      dismissInfobarModal:self
                 animated:NO
               completion:^{
                 [self.dispatcher showSavedPasswordsSettingsFromViewController:
                                      self.baseViewController];
               }];
}

@end

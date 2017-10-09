// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/adaptor/application_commands_adaptor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ApplicationCommandsAdaptor

@synthesize viewControllerForAlert = _viewControllerForAlert;

#pragma mark - ApplicationCommands

- (void)dismissModalDialogs {
  [self showAlert:@"dismissModalDialogs"];
}

- (void)showSettings {
  [self showAlert:@"showSettings"];
}

- (void)switchModesAndOpenNewTab:(OpenNewTabCommand*)newTabCommand {
  [self showAlert:@"switchModesAndOpenNewTab"];
}

- (void)startVoiceSearch:(StartVoiceSearchCommand*)command {
  [self showAlert:@"startVoiceSearch"];
}

- (void)showHistory {
  [self showAlert:@"showHistory"];
}

- (void)closeSettingsUIAndOpenURL:(OpenUrlCommand*)command {
  [self showAlert:@"closeSettingsUIAndOpenURL"];
}

- (void)closeSettingsUI {
  [self showAlert:@"closeSettingsUI"];
}

- (void)displayTabSwitcher {
  [self showAlert:@"displayTabSwitcher"];
}

- (void)dismissTabSwitcher {
  [self showAlert:@"dismissTabSwitcher"];
}

- (void)showClearBrowsingDataSettings {
  [self showAlert:@"showClearBrowsingDataSettings"];
}

- (void)showAutofillSettings {
  [self showAlert:@"showAutofillSettings"];
}

- (void)showSavePasswordsSettings {
  [self showAlert:@"showSavePasswordsSettings"];
}

- (void)showReportAnIssue {
  [self showAlert:@"showReportAnIssue"];
}

- (void)openURL:(OpenUrlCommand*)command {
  [self showAlert:@"openURL"];
}

- (void)showSignin:(ShowSigninCommand*)command {
  [self showAlert:@"showSignin"];
}

- (void)showAddAccount {
  [self showAlert:@"showAddAccount"];
}

#pragma mark - ApplicationSettingsCommands

- (void)showAccountsSettings {
  [self showAlert:@"showAccountsSettings"];
}

- (void)showSyncSettings {
  [self showAlert:@"showSyncSettings"];
}

- (void)showSyncPassphraseSettings {
  [self showAlert:@"showSyncPassphraseSettings"];
}

#pragma mark - Private

// TODO(crbug.com/740793): Remove this method once no method is using it.
- (void)showAlert:(NSString*)message {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:message
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.viewControllerForAlert presentViewController:alertController
                                            animated:YES
                                          completion:nil];
}

@end

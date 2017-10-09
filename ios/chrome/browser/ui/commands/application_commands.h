// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_

#import <Foundation/Foundation.h>

@class OpenNewTabCommand;
@class OpenUrlCommand;
@class ShowSigninCommand;
@class StartVoiceSearchCommand;

// This protocol groups commands that are part of ApplicationCommands, but
// may also be forwarded directly to a settings navigation controller.
@protocol ApplicationSettingsCommands

// Shows the accounts settings.
- (void)showAccountsSettings;

// Shows the sync settings UI.
- (void)showSyncSettings;

// Shows the sync encryption passphrase UI.
- (void)showSyncPassphraseSettings;

@end

// Protocol for commands that will generally be handled by the application,
// rather than a specific tab; in practice this means the MainController
// instance.
// This protocol includes all of the methods in ApplicationSettingsCommands; an
// object that implements the methods in this protocol should be able to forward
// ApplicationSettingsCommands to the settings view controller if necessary.

@protocol ApplicationCommands<NSObject, ApplicationSettingsCommands>

// Dismisses all modal dialogs.
- (void)dismissModalDialogs;

// Shows the Settings UI.
- (void)showSettings;

// Switches to show either regular or incognito tabs, and then opens
// a new oen of those tabs. |newTabCommand|'s |incognito| property inidcates
// the type of tab to open.
- (void)switchModesAndOpenNewTab:(OpenNewTabCommand*)newTabCommand;

// Starts a voice search on the current BVC.
- (void)startVoiceSearch:(StartVoiceSearchCommand*)command;

// Shows the History UI.
- (void)showHistory;

// Closes the History UI and opens a URL.
- (void)closeSettingsUIAndOpenURL:(OpenUrlCommand*)command;

// Closes the History UI.
- (void)closeSettingsUI;

// Shows the TabSwitcher UI.
- (void)displayTabSwitcher;

// Dismisses the TabSwitcher UI.
- (void)dismissTabSwitcher;

// Shows the Clear Browsing Data Settings UI (part of Settings).
- (void)showClearBrowsingDataSettings;

// Shows the Autofill Settings UI.
- (void)showAutofillSettings;

// Shows the Save Passwords settings UI.
- (void)showSavePasswordsSettings;

// Shows the Report an Issue UI.
- (void)showReportAnIssue;

// Opens the |command| URL.
- (void)openURL:(OpenUrlCommand*)command;

// Shows the signin UI.
- (void)showSignin:(ShowSigninCommand*)command;

// Shows the Add Account UI
- (void)showAddAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_

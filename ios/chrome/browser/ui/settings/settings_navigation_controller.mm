// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/settings/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/passwords_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/browser/ui/settings/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync_settings_collection_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsNavigationController () <
    GoogleServicesSettingsCoordinatorDelegate>

// Google services settings coordinator.
@property(nonatomic, strong)
    GoogleServicesSettingsCoordinator* googleServicesSettingsCoordinator;

@end

@implementation SettingsNavigationController {
  ios::ChromeBrowserState* mainBrowserState_;  // weak
  __weak id<SettingsNavigationControllerDelegate> delegate_;
}

@synthesize googleServicesSettingsCoordinator =
    _googleServicesSettingsCoordinator;
@synthesize shouldCommitSyncChangesOnDismissal =
    shouldCommitSyncChangesOnDismissal_;

#pragma mark - SettingsNavigationController methods.

+ (SettingsNavigationController*)
newSettingsMainControllerWithBrowserState:(ios::ChromeBrowserState*)browserState
                                 delegate:
                                     (id<SettingsNavigationControllerDelegate>)
                                         delegate {
  SettingsCollectionViewController* controller =
      [[SettingsCollectionViewController alloc]
          initWithBrowserState:browserState
                    dispatcher:[delegate dispatcherForSettings]];
  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];
  [controller navigationItem].rightBarButtonItem = [nc doneButton];
  return nc;
}

+ (SettingsNavigationController*)
newAccountsController:(ios::ChromeBrowserState*)browserState
             delegate:(id<SettingsNavigationControllerDelegate>)delegate {
  AccountsTableViewController* controller =
      [[AccountsTableViewController alloc] initWithBrowserState:browserState
                                      closeSettingsOnAddAccount:YES];
  controller.dispatcher = [delegate dispatcherForSettings];
  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];
  [controller navigationItem].leftBarButtonItem = [nc closeButton];
  return nc;
}

+ (SettingsNavigationController*)
     newSyncController:(ios::ChromeBrowserState*)browserState
allowSwitchSyncAccount:(BOOL)allowSwitchSyncAccount
              delegate:(id<SettingsNavigationControllerDelegate>)delegate {
  SyncSettingsCollectionViewController* controller =
      [[SyncSettingsCollectionViewController alloc]
            initWithBrowserState:browserState
          allowSwitchSyncAccount:allowSwitchSyncAccount];
  controller.dispatcher = [delegate dispatcherForSettings];
  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];
  [controller navigationItem].rightBarButtonItem = [nc doneButton];
  return nc;
}

+ (SettingsNavigationController*)
newUserFeedbackController:(ios::ChromeBrowserState*)browserState
                 delegate:(id<SettingsNavigationControllerDelegate>)delegate
       feedbackDataSource:(id<UserFeedbackDataSource>)dataSource {
  DCHECK(ios::GetChromeBrowserProvider()
             ->GetUserFeedbackProvider()
             ->IsUserFeedbackEnabled());
  UIViewController* controller = ios::GetChromeBrowserProvider()
                                     ->GetUserFeedbackProvider()
                                     ->CreateViewController(dataSource);
  DCHECK(controller);
  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];
  return nc;
}

+ (SettingsNavigationController*)
newSyncEncryptionPassphraseController:(ios::ChromeBrowserState*)browserState
                             delegate:(id<SettingsNavigationControllerDelegate>)
                                          delegate {
  SyncEncryptionPassphraseTableViewController* controller =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowserState:browserState];
  controller.dispatcher = [delegate dispatcherForSettings];
  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];
  [controller navigationItem].leftBarButtonItem = [nc closeButton];
  return nc;
}

+ (SettingsNavigationController*)
newSavePasswordsController:(ios::ChromeBrowserState*)browserState
                  delegate:(id<SettingsNavigationControllerDelegate>)delegate {
  PasswordsTableViewController* controller =
      [[PasswordsTableViewController alloc] initWithBrowserState:browserState];
  controller.dispatcher = [delegate dispatcherForSettings];

  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];
  [controller navigationItem].rightBarButtonItem = [nc doneButton];

  // Make sure the close button is always present, as the Save Passwords screen
  // isn't just shown from Settings.
  [controller navigationItem].leftBarButtonItem = [nc closeButton];
  return nc;
}

+ (SettingsNavigationController*)
newImportDataController:(ios::ChromeBrowserState*)browserState
               delegate:(id<SettingsNavigationControllerDelegate>)delegate
     importDataDelegate:(id<ImportDataControllerDelegate>)importDataDelegate
              fromEmail:(NSString*)fromEmail
                toEmail:(NSString*)toEmail
             isSignedIn:(BOOL)isSignedIn {
  UIViewController* controller =
      [[ImportDataTableViewController alloc] initWithDelegate:importDataDelegate
                                                    fromEmail:fromEmail
                                                      toEmail:toEmail
                                                   isSignedIn:isSignedIn];

  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];

  // Make sure the close button is always present, as the Save Passwords screen
  // isn't just shown from Settings.
  [controller navigationItem].leftBarButtonItem = [nc closeButton];
  return nc;
}

+ (SettingsNavigationController*)
newAutofillProfilleController:(ios::ChromeBrowserState*)browserState
                     delegate:
                         (id<SettingsNavigationControllerDelegate>)delegate {
  AutofillProfileTableViewController* controller =
      [[AutofillProfileTableViewController alloc]
          initWithBrowserState:browserState];
  controller.dispatcher = [delegate dispatcherForSettings];

  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];

  // Make sure the close button is always present, as the Autofill screen
  // isn't just shown from Settings.
  [controller navigationItem].leftBarButtonItem = [nc closeButton];
  return nc;
}

+ (SettingsNavigationController*)
newAutofillCreditCardController:(ios::ChromeBrowserState*)browserState
                       delegate:
                           (id<SettingsNavigationControllerDelegate>)delegate {
  AutofillCreditCardTableViewController* controller =
      [[AutofillCreditCardTableViewController alloc]
          initWithBrowserState:browserState];
  controller.dispatcher = [delegate dispatcherForSettings];

  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];

  // Make sure the close button is always present, as the Autofill screen
  // isn't just shown from Settings.
  [controller navigationItem].leftBarButtonItem = [nc closeButton];
  return nc;
}

#pragma mark - Lifecycle

- (instancetype)
initWithRootViewController:(UIViewController*)rootViewController
              browserState:(ios::ChromeBrowserState*)browserState
                  delegate:(id<SettingsNavigationControllerDelegate>)delegate {
  DCHECK(browserState);
  DCHECK(!browserState->IsOffTheRecord());
  self = rootViewController
             ? [super initWithRootViewController:rootViewController]
             : [super init];
  if (self) {
    mainBrowserState_ = browserState;
    delegate_ = delegate;
    shouldCommitSyncChangesOnDismissal_ = YES;
    [self setModalPresentationStyle:UIModalPresentationFormSheet];
    [self setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.navigationBar.prefersLargeTitles = YES;
  self.navigationBar.accessibilityIdentifier = @"SettingNavigationBar";
}

#pragma mark - Public

- (UIBarButtonItem*)doneButton {
  UIBarButtonItem* item = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(closeSettings)];
  item.accessibilityIdentifier = kSettingsDoneButtonId;
  return item;
}

- (void)settingsWillBeDismissed {
  // Notify all controllers that settings are about to be dismissed.
  for (UIViewController* controller in [self viewControllers]) {
    if ([controller respondsToSelector:@selector(settingsWillBeDismissed)]) {
      [controller performSelector:@selector(settingsWillBeDismissed)];
    }
  }

  // Sync changes cannot be cancelled and they must always be committed when
  // existing settings.
  if (shouldCommitSyncChangesOnDismissal_) {
    SyncSetupServiceFactory::GetForBrowserState([self mainBrowserState])
        ->CommitChanges();
  }

  // Reset the delegate to prevent any queued transitions from attempting to
  // close the settings.
  delegate_ = nil;
}

- (void)closeSettings {
  [delegate_ closeSettings];
}

- (void)popViewControllerOrCloseSettingsAnimated:(BOOL)animated {
  if (self.viewControllers.count > 1) {
    // Pop the top view controller to reveal the view controller underneath.
    [self popViewControllerAnimated:animated];
  } else {
    // If there is only one view controller in the navigation stack,
    // simply close settings.
    [self closeSettings];
  }
}

#pragma mark - Private

// Creates an autoreleased "X" button that closes the settings when tapped.
- (UIBarButtonItem*)closeButton {
  UIBarButtonItem* closeButton =
      [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon closeIcon]
                                          target:self
                                          action:@selector(closeSettings)];
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_CLOSE);
  return closeButton;
}

#pragma mark - GoogleServicesSettingsCoordinatorDelegate

- (void)googleServicesSettingsCoordinatorDidRemove:
    (GoogleServicesSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.googleServicesSettingsCoordinator, coordinator);
  self.googleServicesSettingsCoordinator = nil;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  UIViewController* poppedController = [self popViewControllerAnimated:YES];
  if (!poppedController)
    [self closeSettings];
  return YES;
}

#pragma mark - UINavigationController

// Ensures that the keyboard is always dismissed during a navigation transition.
- (BOOL)disablesAutomaticKeyboardDismissal {
  return NO;
}

#pragma mark - UIResponder

- (NSArray*)keyCommands {
  if ([self presentedViewController]) {
    return nil;
  }
  __weak SettingsNavigationController* weakSelf = self;
  return @[
    [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                           modifierFlags:Cr_UIKeyModifierNone
                                   title:nil
                                  action:^{
                                    [weakSelf closeSettings];
                                  }],
  ];
}

#pragma mark - ApplicationSettingsCommands

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showAccountsSettingsFromViewController:
    (UIViewController*)baseViewController {
  AccountsTableViewController* controller = [[AccountsTableViewController alloc]
           initWithBrowserState:mainBrowserState_
      closeSettingsOnAddAccount:NO];
  controller.dispatcher = [delegate_ dispatcherForSettings];
  [self pushViewController:controller animated:YES];
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  if ([self.topViewController
          isKindOfClass:[GoogleServicesSettingsViewController class]]) {
    // The top view controller is already the Google services settings panel.
    // No need to open it.
    return;
  }
  self.googleServicesSettingsCoordinator =
      [[GoogleServicesSettingsCoordinator alloc]
          initWithBaseViewController:self
                        browserState:mainBrowserState_];
  self.googleServicesSettingsCoordinator.dispatcher =
      [delegate_ dispatcherForSettings];
  self.googleServicesSettingsCoordinator.navigationController = self;
  self.googleServicesSettingsCoordinator.delegate = self;
  [self.googleServicesSettingsCoordinator start];
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController {
  SyncSettingsCollectionViewController* controller =
      [[SyncSettingsCollectionViewController alloc]
            initWithBrowserState:mainBrowserState_
          allowSwitchSyncAccount:YES];
  controller.dispatcher = [delegate_ dispatcherForSettings];
  [self pushViewController:controller animated:YES];
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  SyncEncryptionPassphraseTableViewController* controller =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowserState:mainBrowserState_];
  controller.dispatcher = [delegate_ dispatcherForSettings];
  [self pushViewController:controller animated:YES];
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController {
  PasswordsTableViewController* controller =
      [[PasswordsTableViewController alloc]
          initWithBrowserState:mainBrowserState_];
  controller.dispatcher = [delegate_ dispatcherForSettings];
  [self pushViewController:controller animated:YES];
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  AutofillProfileTableViewController* controller =
      [[AutofillProfileTableViewController alloc]
          initWithBrowserState:mainBrowserState_];
  controller.dispatcher = [delegate_ dispatcherForSettings];
  [self pushViewController:controller animated:YES];
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showCreditCardSettingsFromViewController:
    (UIViewController*)baseViewController {
  AutofillCreditCardTableViewController* controller =
      [[AutofillCreditCardTableViewController alloc]
          initWithBrowserState:mainBrowserState_];
  controller.dispatcher = [delegate_ dispatcherForSettings];
  [self pushViewController:controller animated:YES];
}

#pragma mark - Profile

- (ios::ChromeBrowserState*)mainBrowserState {
  return mainBrowserState_;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

@end

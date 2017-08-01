// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/clear_browsing_data_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/material_components/app_bar_presenting.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/settings/accounts_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/import_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/save_passwords_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/browser/ui/settings/sync_encryption_passphrase_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync_settings_collection_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/620361): Remove the entire class when iOS 9 is dropped.
@interface SettingsAppBarContainerViewController
    : MDCAppBarContainerViewController
@end

@implementation SettingsAppBarContainerViewController

#pragma mark - Status bar

- (UIViewController*)childViewControllerForStatusBarHidden {
  if (!base::ios::IsRunningOnIOS10OrLater()) {
    // TODO(crbug.com/620361): Remove the entire method override when iOS 9 is
    // dropped.
    return self.contentViewController;
  } else {
    return [super childViewControllerForStatusBarHidden];
  }
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  if (!base::ios::IsRunningOnIOS10OrLater()) {
    // TODO(crbug.com/620361): Remove the entire method override when iOS 9 is
    // dropped.
    return self.contentViewController;
  } else {
    return [super childViewControllerForStatusBarStyle];
  }
}

@end

@interface SettingsNavigationController ()<UIGestureRecognizerDelegate>

// Sets up the UI.  Used by both initializers.
- (void)configureUI;

// Closes the settings by calling |closeSettings| on |delegate|.
- (void)closeSettings;

// Creates an autoreleased Back button for a UINavigationItem which will pop the
// top view controller when it is pressed. Should only be called by view
// controllers owned by SettingsNavigationController.
- (UIBarButtonItem*)backButton;

// Creates an autoreleased "X" button that closes the settings when tapped.
- (UIBarButtonItem*)closeButton;

// Creates an autoreleased "CANCEL" button that closes the settings when tapped.
- (UIBarButtonItem*)cancelButton;

// Intercepts the chrome command |sender|. If |sender| is an
// |IDC_CLOSE_SETTINGS_AND_OPEN_URL| and |delegate_| is not nil, then it
// calls [delegate closeSettingsAndOpenUrl:sender], otherwise it forwards the
// command up the responder chain.
- (void)chromeExecuteCommand:(id)sender;

@end

@implementation SettingsNavigationController {
  ios::ChromeBrowserState* mainBrowserState_;  // weak
  __weak id<SettingsNavigationControllerDelegate> delegate_;
  // Keeps a mapping between the view controllers that are wrapped to display an
  // app bar and the containers that wrap them.
  NSMutableDictionary* appBarContainedViewControllers_;
}

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
  UIViewController* controller = [[AccountsCollectionViewController alloc]
           initWithBrowserState:browserState
      closeSettingsOnAddAccount:YES
                     dispatcher:[delegate dispatcherForSettings]];
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
  UIViewController* controller = [[SyncSettingsCollectionViewController alloc]
        initWithBrowserState:browserState
      allowSwitchSyncAccount:allowSwitchSyncAccount];
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
  [controller navigationItem].rightBarButtonItem = [nc cancelButton];
  return nc;
}

+ (SettingsNavigationController*)
newClearBrowsingDataController:(ios::ChromeBrowserState*)browserState
                      delegate:
                          (id<SettingsNavigationControllerDelegate>)delegate {
  UIViewController* controller =
      [[ClearBrowsingDataCollectionViewController alloc]
          initWithBrowserState:browserState];
  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];
  [controller navigationItem].rightBarButtonItem = [nc doneButton];
  return nc;
}

+ (SettingsNavigationController*)
newSyncEncryptionPassphraseController:(ios::ChromeBrowserState*)browserState
                             delegate:(id<SettingsNavigationControllerDelegate>)
                                          delegate {
  UIViewController* controller =
      [[SyncEncryptionPassphraseCollectionViewController alloc]
          initWithBrowserState:browserState];
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
  UIViewController* controller = [[SavePasswordsCollectionViewController alloc]
      initWithBrowserState:browserState];

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
  UIViewController* controller = [[ImportDataCollectionViewController alloc]
      initWithDelegate:importDataDelegate
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
newAutofillController:(ios::ChromeBrowserState*)browserState
             delegate:(id<SettingsNavigationControllerDelegate>)delegate {
  UIViewController* controller = [[AutofillCollectionViewController alloc]
      initWithBrowserState:browserState];

  SettingsNavigationController* nc = [[SettingsNavigationController alloc]
      initWithRootViewController:controller
                    browserState:browserState
                        delegate:delegate];
  [controller navigationItem].rightBarButtonItem = [nc doneButton];

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
    [self configureUI];
  }
  return self;
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

- (void)back {
  [self popViewControllerAnimated:YES];
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

- (void)configureUI {
  [self setModalPresentationStyle:UIModalPresentationFormSheet];
  [self setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  // Since the navigation bar is hidden, the gesture to swipe to go back can
  // become inactive. Setting the delegate to self is an MDC workaround to have
  // it consistently work with AppBar.
  // https://github.com/material-components/material-components-ios/issues/720
  [self setNavigationBarHidden:YES];
  [self.interactivePopGestureRecognizer setDelegate:self];
}

- (BOOL)hasRightDoneButton {
  UIBarButtonItem* rightButton =
      self.topViewController.navigationItem.rightBarButtonItem;
  if (!rightButton)
    return NO;
  UIBarButtonItem* doneButton = [self doneButton];
  return [rightButton style] == [doneButton style] &&
         [[rightButton title] compare:[doneButton title]] == NSOrderedSame;
}

- (UIBarButtonItem*)backButton {
  // Create a custom Back bar button item, as Material Navigation Bar deprecated
  // the back arrow with a shaft.
  return [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                             target:self
                                             action:@selector(back)];
}

- (UIBarButtonItem*)doneButton {
  // Create a custom Done bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemDone item.
  return [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(closeSettings)];
}

- (UIBarButtonItem*)closeButton {
  UIBarButtonItem* closeButton =
      [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon closeIcon]
                                          target:self
                                          action:@selector(closeSettings)];
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_CLOSE);
  return closeButton;
}

- (UIBarButtonItem*)cancelButton {
  // Create a custom Cancel bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemCancel item.
  return [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(closeSettings)];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return [self.topViewController supportedInterfaceOrientations];
}

- (BOOL)shouldAutorotate {
  return [self.topViewController shouldAutorotate];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  DCHECK_EQ(gestureRecognizer, self.interactivePopGestureRecognizer);
  return self.viewControllers.count > 1;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  UIViewController* poppedController = [self popViewControllerAnimated:YES];
  if (!poppedController)
    [self closeSettings];
  return YES;
}

#pragma mark - UINavigationController

- (void)pushViewController:(UIViewController*)viewController
                  animated:(BOOL)animated {
  // Add a back button if the view controller is not the root view controller
  // and doesn’t already have a left bar button item.
  if (self.viewControllers.count > 0 &&
      viewController.navigationItem.leftBarButtonItems.count == 0) {
    viewController.navigationItem.leftBarButtonItem = [self backButton];
  }
  // Wrap the view controller in an MDCAppBarContainerViewController if needed.
  [super pushViewController:[self wrappedControllerIfNeeded:viewController]
                   animated:animated];
}

- (UIViewController*)popViewControllerAnimated:(BOOL)animated {
  UIViewController* viewController = [super popViewControllerAnimated:animated];
  // Unwrap the view controller from its MDCAppBarContainerViewController if
  // needed.
  return [self unwrappedControllerIfNeeded:viewController];
}

- (NSArray*)popToViewController:(UIViewController*)viewController
                       animated:(BOOL)animated {
  // First, check if the view controller was wrapped in an app bar container.
  MDCAppBarContainerViewController* appBarContainer =
      [self appBarContainerForController:viewController];
  // Pop the view controllers.
  NSArray* poppedViewControllers =
      [super popToViewController:appBarContainer ?: viewController
                        animated:animated];
  // Unwrap the popped view controllers from their
  // MDCAppBarContainerViewController if needed.
  NSMutableArray* viewControllers = [NSMutableArray array];
  [poppedViewControllers
      enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL* stop) {
        [viewControllers
            addObject:[self unwrappedControllerIfNeeded:viewController]];
      }];
  return viewControllers;
}

// Ensures that the keyboard is always dismissed during a navigation transition.
- (BOOL)disablesAutomaticKeyboardDismissal {
  return NO;
}

#pragma mark - UIResponder (ChromeExecuteCommand)

- (void)chromeExecuteCommand:(id)sender {
  switch ([sender tag]) {
    case IDC_CLOSE_SETTINGS: {
      [delegate_ closeSettings];
      return;
    }
    case IDC_OPEN_URL:
      NOTREACHED() << "You should probably use the command "
                   << "IDC_CLOSE_SETTINGS_AND_OPEN_URL instead of IDC_OPEN_URL";
    case IDC_CLOSE_SETTINGS_AND_OPEN_URL: {
      [delegate_ closeSettingsAndOpenUrl:sender];
      return;
    }
    case IDC_SHOW_SIGNIN_IOS:
      // Sign-in actions can only happen on the main browser state (not on
      // incognito browser state), which is unique. The command can just be
      // forwarded up the responder chain.
      break;
    case IDC_CLEAR_BROWSING_DATA_IOS: {
      // Check that the data for the right browser state is being cleared before
      // forwarding it up the responder chain.
      ios::ChromeBrowserState* commandBrowserState =
          [base::mac::ObjCCast<ClearBrowsingDataCommand>(sender) browserState];

      // Clearing browsing data for the wrong profile is a destructive action.
      // Executing it on the wrong profile is a privacy issue. Kill the
      // app if this ever happens.
      CHECK_EQ(commandBrowserState, [self mainBrowserState]);
      break;
    }
    case IDC_SHOW_SYNC_SETTINGS: {
      UIViewController* controller =
          [[SyncSettingsCollectionViewController alloc]
                initWithBrowserState:mainBrowserState_
              allowSwitchSyncAccount:YES];
      [self pushViewController:controller animated:YES];
      return;
    }
    case IDC_SHOW_SYNC_PASSPHRASE_SETTINGS: {
      UIViewController* controller =
          [[SyncEncryptionPassphraseCollectionViewController alloc]
              initWithBrowserState:mainBrowserState_];
      [self pushViewController:controller animated:YES];
      return;
    }
    default:
      NOTREACHED()
          << "Unexpected command " << [sender tag]
          << " Settings commands must execute on the main browser state.";
  }
  [[self nextResponder] chromeExecuteCommand:sender];
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

- (void)showAccountsSettings {
  UIViewController* controller = [[AccountsCollectionViewController alloc]
           initWithBrowserState:mainBrowserState_
      closeSettingsOnAddAccount:NO
                     dispatcher:[delegate_ dispatcherForSettings]];
  [self pushViewController:controller animated:YES];
}

#pragma mark - Profile

- (ios::ChromeBrowserState*)mainBrowserState {
  return mainBrowserState_;
}

#pragma mark - Status bar

- (BOOL)modalPresentationCapturesStatusBarAppearance {
  if (!base::ios::IsRunningOnIOS10OrLater()) {
    // TODO(crbug.com/620361): Remove the entire method override when iOS 9 is
    // dropped.
    return YES;
  } else {
    return [super modalPresentationCapturesStatusBarAppearance];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (!base::ios::IsRunningOnIOS10OrLater()) {
    // TODO(crbug.com/620361): Remove the entire method override when iOS 9 is
    // dropped.
    [self setNeedsStatusBarAppearanceUpdate];
  }
}

#pragma mark - AppBar Containment

// If viewController doesn't implement the AppBarPresenting protocol, it is
// wrapped in an MDCAppBarContainerViewController, which is returned. Otherwise,
// viewController is returned.
- (UIViewController*)wrappedControllerIfNeeded:(UIViewController*)controller {
  // If the controller can't be presented with an app bar, it needs to be
  // wrapped in an MDCAppBarContainerViewController.
  if (![controller conformsToProtocol:@protocol(AppBarPresenting)]) {
    MDCAppBarContainerViewController* appBarContainer =
        [[SettingsAppBarContainerViewController alloc]
            initWithContentViewController:controller];

    // Configure the style.
    ConfigureAppBarWithCardStyle(appBarContainer.appBar);

    // Adjust the frame of the contained view controller's view to be below the
    // app bar.
    CGRect contentFrame = controller.view.frame;
    CGSize headerSize = [appBarContainer.appBar.headerViewController.headerView
        sizeThatFits:contentFrame.size];
    contentFrame = UIEdgeInsetsInsetRect(
        contentFrame, UIEdgeInsetsMake(headerSize.height, 0, 0, 0));
    controller.view.frame = contentFrame;

    // Register the app bar container and return it.
    [self registerAppBarContainer:appBarContainer];
    return appBarContainer;
  } else {
    return controller;
  }
}

// If controller is an MDCAppBarContainerViewController, it returns its content
// view controller. Otherwise, it returns viewController.
- (UIViewController*)unwrappedControllerIfNeeded:(UIViewController*)controller {
  MDCAppBarContainerViewController* potentialAppBarController =
      base::mac::ObjCCast<MDCAppBarContainerViewController>(controller);
  if (potentialAppBarController) {
    // Unregister the app bar container and return it.
    [self unregisterAppBarContainer:potentialAppBarController];
    return [potentialAppBarController contentViewController];
  } else {
    return controller;
  }
}

// Adds an app bar container in a dictionary mapping its content view
// controller's pointer to itself.
- (void)registerAppBarContainer:(MDCAppBarContainerViewController*)container {
  if (!appBarContainedViewControllers_) {
    appBarContainedViewControllers_ = [[NSMutableDictionary alloc] init];
  }
  NSValue* key = [self keyForController:[container contentViewController]];
  [appBarContainedViewControllers_ setObject:container forKey:key];
}

// Removes the app bar container entry from the aforementioned dictionary.
- (void)unregisterAppBarContainer:(MDCAppBarContainerViewController*)container {
  NSValue* key = [self keyForController:[container contentViewController]];
  [appBarContainedViewControllers_ removeObjectForKey:key];
}

// Returns the app bar container containing |controller| if it is contained.
// Otherwise, returns nil.
- (MDCAppBarContainerViewController*)appBarContainerForController:
    (UIViewController*)controller {
  NSValue* key = [self keyForController:controller];
  return [appBarContainedViewControllers_ objectForKey:key];
}

// Returns the dictionary key to use when dealing with |controller|.
- (NSValue*)keyForController:(UIViewController*)controller {
  return [NSValue valueWithNonretainedObject:controller];
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

@end

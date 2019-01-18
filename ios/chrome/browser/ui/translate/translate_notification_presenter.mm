// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/translate_notification_presenter.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/translate/translate_notification_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSnackbarActionAccessibilityIdentifier =
    @"SnackbarActionAccessibilityIdentifier";

NSString* const kTranslateNotificationSnackbarCategory =
    @"TranslateNotificationSnackbarCategory";

}  // namespace

@implementation TranslateNotificationPresenter

#pragma mark - TranslateNotificationHandler

- (void)showAlwaysTranslateLanguageNotificationWithDelegate:
            (id<TranslateNotificationDelegate>)delegate
                                             sourceLanguage:
                                                 (NSString*)sourceLanguage
                                             targetLanguage:
                                                 (NSString*)targetLanguage {
  base::string16 sourceLanguageName = base::SysNSStringToUTF16(sourceLanguage);
  base::string16 targetLanguageName = base::SysNSStringToUTF16(targetLanguage);
  NSString* text = base::SysUTF16ToNSString(
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_NOTIFICATION_ALWAYS_TRANSLATE,
                                 sourceLanguageName, targetLanguageName));

  __weak id<TranslateNotificationDelegate> weakDelegate = delegate;
  __weak TranslateNotificationPresenter* weakSelf = self;
  __block BOOL action_block_executed = NO;
  auto action = ^() {
    action_block_executed = YES;
  };
  auto completion = ^(BOOL userInitiated) {
    // Inform the delegate of the dismissal unless the user tapped "Undo".
    if (action_block_executed)
      return;
    [weakDelegate
        translateNotificationHandlerDidDismissAlwaysTranslateLanguage:weakSelf];
  };
  [self showSnackbarWithText:text
               actionHandler:action
           completionHandler:completion];
}

- (void)showNeverTranslateLanguageNotificationWithDelegate:
            (id<TranslateNotificationDelegate>)delegate
                                            sourceLanguage:
                                                (NSString*)sourceLanguage {
  base::string16 sourceLanguageName = base::SysNSStringToUTF16(sourceLanguage);
  NSString* text = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_TRANSLATE_NOTIFICATION_LANGUAGE_NEVER, sourceLanguageName));

  __weak id<TranslateNotificationDelegate> weakDelegate = delegate;
  __weak TranslateNotificationPresenter* weakSelf = self;
  __block BOOL action_block_executed = NO;
  auto action = ^() {
    // Inform the delegate of the dismissal unless the user tapped "Undo".
    action_block_executed = YES;
  };
  auto completion = ^(BOOL userInitiated) {
    if (action_block_executed)
      return;
    [weakDelegate
        translateNotificationHandlerDidDismissNeverTranslateLanguage:weakSelf];
  };
  [self showSnackbarWithText:text
               actionHandler:action
           completionHandler:completion];
}

- (void)showNeverTranslateSiteNotificationWithDelegate:
    (id<TranslateNotificationDelegate>)delegate {
  NSString* text =
      l10n_util::GetNSString(IDS_TRANSLATE_NOTIFICATION_SITE_NEVER);

  __weak id<TranslateNotificationDelegate> weakDelegate = delegate;
  __weak TranslateNotificationPresenter* weakSelf = self;
  __block BOOL action_block_executed = NO;
  auto action = ^() {
    action_block_executed = YES;
  };
  auto completion = ^(BOOL userInitiated) {
    // Inform the delegate of the dismissal unless the user tapped "Undo".
    if (action_block_executed)
      return;
    [weakDelegate
        translateNotificationHandlerDidDismissNeverTranslateSite:weakSelf];
  };
  [self showSnackbarWithText:text
               actionHandler:action
           completionHandler:completion];
}

- (void)dismissNotification {
  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:
                          kTranslateNotificationSnackbarCategory];
}

#pragma mark - Private

- (void)showSnackbarWithText:(NSString*)text
               actionHandler:(void (^)())actionHandler
           completionHandler:(void (^)(BOOL))completionHandler {
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.title = l10n_util::GetNSString(IDS_TRANSLATE_NOTIFICATION_UNDO);
  action.accessibilityIdentifier = kSnackbarActionAccessibilityIdentifier;
  action.handler = actionHandler;
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.action = action;
  message.completionHandler = completionHandler;
  message.category = kTranslateNotificationSnackbarCategory;
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  [MDCSnackbarManager showMessage:message];
}

@end

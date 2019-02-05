// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_infobar_controller.h"

#import <UIKit/UIKit.h>

#include <stddef.h>
#include <memory>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_controller+protected.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#include "ios/chrome/browser/translate/language_selection_context.h"
#include "ios/chrome/browser/translate/language_selection_delegate.h"
#include "ios/chrome/browser/translate/language_selection_handler.h"
#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"
#include "ios/chrome/browser/translate/translate_option_selection_delegate.h"
#include "ios/chrome/browser/translate/translate_option_selection_handler.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_view.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_view_delegate.h"
#include "ios/chrome/browser/ui/translate/translate_notification_delegate.h"
#include "ios/chrome/browser/ui/translate/translate_notification_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Whether langugage selection popup menu is offered; and whether it is to
// select the source or the target language.
typedef NS_ENUM(NSInteger, LanguageSelectionState) {
  LanguageSelectionStateNone,
  LanguageSelectionStateSource,
  LanguageSelectionStateTarget,
};

// Various user actions to keep track of.
typedef NS_OPTIONS(NSUInteger, UserAction) {
  UserActionNone = 0,
  UserActionTranslate = 1 << 0,
  UserActionRevert = 1 << 1,
  UserActionAlwaysTranslate = 1 << 2,
  UserActionNeverTranslateLanguage = 1 << 3,
  UserActionNeverTranslateSite = 1 << 4,
  UserActionExpandMenu = 1 << 5,
};

}  // namespace

@interface TranslateInfoBarController () <LanguageSelectionDelegate,
                                          TranslateInfobarDelegateObserving,
                                          TranslateInfobarViewDelegate,
                                          TranslateNotificationDelegate,
                                          TranslateOptionSelectionDelegate> {
  std::unique_ptr<TranslateInfobarDelegateObserverBridge>
      _translateInfobarDelegateObserver;
}

// Overrides superclass property.
@property(nonatomic, readonly)
    translate::TranslateInfoBarDelegate* infoBarDelegate;

@property(nonatomic, weak) TranslateInfobarView* infobarView;

// Indicates whether langugage selection popup menu is offered; and whether it
// is to select the source or the target language.
@property(nonatomic, assign) LanguageSelectionState languageSelectionState;

// Tracks user actions.
@property(nonatomic, assign) UserAction userAction;

// The source language name.
@property(nonatomic, readonly) NSString* sourceLanguage;

// The target language name.
@property(nonatomic, readonly) NSString* targetLanguage;

@end

@implementation TranslateInfoBarController

@dynamic infoBarDelegate;

#pragma mark - InfoBarControllerProtocol

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infoBarDelegate {
  self = [super initWithInfoBarDelegate:infoBarDelegate];
  if (self) {
    _translateInfobarDelegateObserver =
        std::make_unique<TranslateInfobarDelegateObserverBridge>(
            infoBarDelegate, self);
    _userAction = UserActionNone;
  }
  return self;
}

- (UIView*)infobarView {
  TranslateInfobarView* infobarView =
      [[TranslateInfobarView alloc] initWithFrame:CGRectZero];
  infobarView.sourceLanguage = self.sourceLanguage;
  infobarView.targetLanguage = self.targetLanguage;
  infobarView.delegate = self;
  _infobarView = infobarView;
  return infobarView;
}

#pragma mark - TranslateInfobarDelegateObserving

- (void)translateInfoBarDelegate:(translate::TranslateInfoBarDelegate*)delegate
          didChangeTranslateStep:(translate::TranslateStep)step
                   withErrorType:(translate::TranslateErrors::Type)errorType {
  switch (step) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      _infobarView.state = TranslateInfobarViewStateBeforeTranslate;
      break;
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      _infobarView.state = TranslateInfobarViewStateTranslating;
      break;
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      _infobarView.state = TranslateInfobarViewStateAfterTranslate;
      break;
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      // Noop.
      break;
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      _infobarView.state = TranslateInfobarViewStateBeforeTranslate;
      [self.translateNotificationHandler showTranslateErrorNotification];
  }
}

- (BOOL)translateInfoBarDelegateDidDismissWithoutInteraction:
    (translate::TranslateInfoBarDelegate*)delegate {
  return self.userAction == UserActionNone;
}

#pragma mark - TranslateInfobarViewDelegate

- (void)translateInfobarViewDidTapSourceLangugage:
    (TranslateInfobarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionRevert;

  self.infoBarDelegate->RevertWithoutClosingInfobar();
  _infobarView.state = TranslateInfobarViewStateBeforeTranslate;
}

- (void)translateInfobarViewDidTapTargetLangugage:
    (TranslateInfobarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionTranslate;

  if (self.infoBarDelegate->ShouldAutoAlwaysTranslate()) {
    // Page will be translated once the snackbar finishes showing.
    [self.translateNotificationHandler
        showAlwaysTranslateLanguageNotificationWithDelegate:self
                                             sourceLanguage:self.sourceLanguage
                                             targetLanguage:
                                                 self.targetLanguage];
  } else {
    self.infoBarDelegate->Translate();
  }
}

- (void)translateInfobarViewDidTapOptions:(TranslateInfobarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionExpandMenu;

  [self showTranslateOptionSelector];
}

- (void)translateInfobarViewDidTapDismiss:(TranslateInfobarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.infoBarDelegate->InfoBarDismissed();

  if (self.infoBarDelegate->ShouldAutoNeverTranslate()) {
    // Infobar will dismiss once the snackbar finishes showing.
    [self.translateNotificationHandler
        showNeverTranslateLanguageNotificationWithDelegate:self
                                            sourceLanguage:self.sourceLanguage];
  } else {
    self.delegate->RemoveInfoBar();
  }
}

#pragma mark - LanguageSelectionDelegate

- (void)languageSelectorSelectedLanguage:(std::string)languageCode {
  if (self.languageSelectionState == LanguageSelectionStateSource) {
    self.infoBarDelegate->UpdateOriginalLanguage(languageCode);
    _infobarView.sourceLanguage = self.sourceLanguage;
  } else {
    self.infoBarDelegate->UpdateTargetLanguage(languageCode);
    _infobarView.targetLanguage = self.targetLanguage;
  }
  self.languageSelectionState = LanguageSelectionStateNone;

  self.infoBarDelegate->Translate();

  [_infobarView updateUIForPopUpMenuDisplayed:NO];
}

- (void)languageSelectorClosedWithoutSelection {
  self.languageSelectionState = LanguageSelectionStateNone;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];
}

#pragma mark - TranslateOptionSelectionDelegate

- (void)popupMenuTableViewControllerDidSelectTargetLanguageSelector:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionExpandMenu;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  self.languageSelectionState = LanguageSelectionStateTarget;
  [self showLanguageSelector];
}

- (void)popupMenuTableViewControllerDidSelectAlwaysTranslateSourceLanguage:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionAlwaysTranslate;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  if (self.infoBarDelegate->ShouldAlwaysTranslate()) {
    self.infoBarDelegate->ToggleAlwaysTranslate();
  } else {
    // Page will be translated once the snackbar finishes showing.
    [self.translateNotificationHandler
        showAlwaysTranslateLanguageNotificationWithDelegate:self
                                             sourceLanguage:self.sourceLanguage
                                             targetLanguage:
                                                 self.targetLanguage];
  }
}

- (void)popupMenuTableViewControllerDidSelectNeverTranslateSourceLanguage:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionNeverTranslateLanguage;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  if (self.infoBarDelegate->IsTranslatableLanguageByPrefs()) {
    // Infobar will dismiss once the snackbar finishes showing.
    [self.translateNotificationHandler
        showNeverTranslateLanguageNotificationWithDelegate:self
                                            sourceLanguage:self.sourceLanguage];
  }
}

- (void)popupMenuTableViewControllerDidSelectNeverTranslateSite:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionNeverTranslateSite;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  if (!self.infoBarDelegate->IsSiteBlacklisted()) {
    // Infobar will dismiss once the snackbar finishes showing.
    [self.translateNotificationHandler
        showNeverTranslateSiteNotificationWithDelegate:self];
  }
}

- (void)popupMenuTableViewControllerDidSelectSourceLanguageSelector:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionExpandMenu;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  self.languageSelectionState = LanguageSelectionStateSource;
  [self showLanguageSelector];
}

- (void)popupMenuPresenterDidCloseWithoutSelection:(PopupMenuPresenter*)sender {
  [_infobarView updateUIForPopUpMenuDisplayed:NO];
}

#pragma mark - TranslateNotificationDelegate

- (void)translateNotificationHandlerDidDismissAlwaysTranslateLanguage:
    (id<TranslateNotificationHandler>)sender {
  self.infoBarDelegate->ToggleAlwaysTranslate();
  self.infoBarDelegate->Translate();
}

- (void)translateNotificationHandlerDidDismissNeverTranslateLanguage:
    (id<TranslateNotificationHandler>)sender {
  self.infoBarDelegate->ToggleTranslatableLanguageByPrefs();
  self.delegate->RemoveInfoBar();
}

- (void)translateNotificationHandlerDidDismissNeverTranslateSite:
    (id<TranslateNotificationHandler>)sender {
  self.infoBarDelegate->ToggleSiteBlacklist();
  self.delegate->RemoveInfoBar();
}

#pragma mark - Properties

- (NSString*)sourceLanguage {
  return base::SysUTF16ToNSString(
      self.infoBarDelegate->original_language_name());
}

- (NSString*)targetLanguage {
  return base::SysUTF16ToNSString(self.infoBarDelegate->target_language_name());
}

#pragma mark - Private

- (void)showTranslateOptionSelector {
  [self.translateOptionSelectionHandler
      showTranslateOptionSelectorWithInfoBarDelegate:self.infoBarDelegate
                                            delegate:self];
  [_infobarView updateUIForPopUpMenuDisplayed:YES];
}

- (void)showLanguageSelector {
  int originalLanguageIndex = -1;
  int targetLanguageIndex = -1;
  for (size_t i = 0; i < self.infoBarDelegate->num_languages(); ++i) {
    if (self.infoBarDelegate->language_code_at(i) ==
        self.infoBarDelegate->original_language_code()) {
      originalLanguageIndex = i;
    }
    if (self.infoBarDelegate->language_code_at(i) ==
        self.infoBarDelegate->target_language_code()) {
      targetLanguageIndex = i;
    }
  }
  DCHECK_GE(originalLanguageIndex, 0);
  DCHECK_GE(targetLanguageIndex, 0);

  size_t selectedIndex;
  size_t disabledIndex;
  if (self.languageSelectionState == LanguageSelectionStateSource) {
    selectedIndex = originalLanguageIndex;
    disabledIndex = targetLanguageIndex;
  } else {
    selectedIndex = targetLanguageIndex;
    disabledIndex = originalLanguageIndex;
  }
  LanguageSelectionContext* context =
      [LanguageSelectionContext contextWithLanguageData:self.infoBarDelegate
                                           initialIndex:selectedIndex
                                       unavailableIndex:disabledIndex];
  [self.languageSelectionHandler showLanguageSelectorWithContext:context
                                                        delegate:self];
  [_infobarView updateUIForPopUpMenuDisplayed:YES];
}

@end

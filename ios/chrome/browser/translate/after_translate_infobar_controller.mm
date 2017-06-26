// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/after_translate_infobar_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#import "ios/chrome/browser/ui/infobars/infobar_view.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
enum AlwaysTranslateSwitchState {
  ALWAYS_TRANSLATE_SWITCH_NOT_CHANGED,
  ALWAYS_TRANSLATE_SWITCH_SET_TO_ENABLED,
  ALWAYS_TRANSLATE_SWITCH_SET_TO_DISABLED,
};
}  // namespace

@interface AfterTranslateInfoBarController () {
  translate::TranslateInfoBarDelegate* _translateInfoBarDelegate;  // weak
  AlwaysTranslateSwitchState _alwaysTranslateSwitchState;
}

// Action for any of the user defined buttons.
- (void)infoBarButtonDidPress:(id)sender;

@end

@implementation AfterTranslateInfoBarController

#pragma mark -
#pragma mark InfoBarControllerProtocol

- (InfoBarView*)viewForDelegate:(infobars::InfoBarDelegate*)delegate
                          frame:(CGRect)frame {
  InfoBarView* infoBarView;
  _translateInfoBarDelegate = delegate->AsTranslateInfoBarDelegate();
  DCHECK(_translateInfoBarDelegate);
  infoBarView =
      [[InfoBarView alloc] initWithFrame:frame delegate:self.delegate];
  // Icon
  gfx::Image icon = _translateInfoBarDelegate->GetIcon();
  if (!icon.IsEmpty())
    [infoBarView addLeftIcon:icon.ToUIImage()];
  // Main text.
  const bool autodeterminedSourceLanguage =
      _translateInfoBarDelegate->original_language_code() ==
      translate::kUnknownLanguageCode;
  bool swappedLanguageButtons;
  std::vector<base::string16> strings;
  translate::TranslateInfoBarDelegate::GetAfterTranslateStrings(
      &strings, &swappedLanguageButtons, autodeterminedSourceLanguage);
  DCHECK_EQ(autodeterminedSourceLanguage ? 2U : 3U, strings.size());
  NSString* label1 = base::SysUTF16ToNSString(strings[0]);
  NSString* label2 = base::SysUTF16ToNSString(strings[1]);
  NSString* label3 = autodeterminedSourceLanguage
                         ? @""
                         : base::SysUTF16ToNSString(strings[2]);
  base::string16 stdOriginal =
      _translateInfoBarDelegate->original_language_name();
  NSString* original = base::SysUTF16ToNSString(stdOriginal);
  NSString* target = base::SysUTF16ToNSString(
      _translateInfoBarDelegate->target_language_name());
  NSString* label =
      [[NSString alloc] initWithFormat:@"%@ %@ %@%@ %@.", label1, original,
                                       label2, label3, target];
  [infoBarView addLabel:label];
  // Close button.
  [infoBarView addCloseButtonWithTag:TranslateInfoBarIOSTag::CLOSE
                              target:self
                              action:@selector(infoBarButtonDidPress:)];
  // Other buttons.
  NSString* buttonRevert = l10n_util::GetNSString(IDS_TRANSLATE_INFOBAR_REVERT);
  NSString* buttonOptions = l10n_util::GetNSString(IDS_DONE);
  [infoBarView addButton1:buttonOptions
                     tag1:TranslateInfoBarIOSTag::AFTER_DONE
                  button2:buttonRevert
                     tag2:TranslateInfoBarIOSTag::AFTER_REVERT
                   target:self
                   action:@selector(infoBarButtonDidPress:)];
  // Always translate switch.
  _alwaysTranslateSwitchState = ALWAYS_TRANSLATE_SWITCH_NOT_CHANGED;
  if (_translateInfoBarDelegate->ShouldShowAlwaysTranslateShortcut()) {
    base::string16 alwaysTranslate = l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE, stdOriginal);
    const BOOL switchValue = _translateInfoBarDelegate->ShouldAlwaysTranslate();
    [infoBarView
        addSwitchWithLabel:base::SysUTF16ToNSString(alwaysTranslate)
                      isOn:switchValue
                       tag:TranslateInfoBarIOSTag::ALWAYS_TRANSLATE_SWITCH
                    target:self
                    action:@selector(infoBarSwitchDidPress:)];
  }
  return infoBarView;
}

#pragma mark - Handling of User Events

- (void)infoBarButtonDidPress:(id)sender {
  // This press might have occurred after the user has already pressed a button,
  // in which case the view has been detached from the delegate and this press
  // should be ignored.
  if (!self.delegate) {
    return;
  }
  if ([sender isKindOfClass:[UIButton class]]) {
    NSUInteger buttonId = static_cast<UIButton*>(sender).tag;
    if (buttonId == TranslateInfoBarIOSTag::CLOSE) {
      self.delegate->InfoBarDidCancel();
    } else {
      DCHECK(buttonId == TranslateInfoBarIOSTag::AFTER_REVERT ||
             buttonId == TranslateInfoBarIOSTag::AFTER_DONE);
      if (buttonId == TranslateInfoBarIOSTag::AFTER_DONE)
        [self saveAlwaysTranslateState];
      self.delegate->InfoBarButtonDidPress(buttonId);
    }
  }
}

- (void)infoBarSwitchDidPress:(id)sender {
  DCHECK_EQ(TranslateInfoBarIOSTag::ALWAYS_TRANSLATE_SWITCH, [sender tag]);
  DCHECK([sender respondsToSelector:@selector(isOn)]);
  _alwaysTranslateSwitchState = [sender isOn]
                                    ? ALWAYS_TRANSLATE_SWITCH_SET_TO_ENABLED
                                    : ALWAYS_TRANSLATE_SWITCH_SET_TO_DISABLED;
}

#pragma mark - Private methods

- (void)saveAlwaysTranslateState {
  if (_alwaysTranslateSwitchState == ALWAYS_TRANSLATE_SWITCH_NOT_CHANGED)
    return;

  const bool alwaysTranslate =
      _alwaysTranslateSwitchState == ALWAYS_TRANSLATE_SWITCH_SET_TO_ENABLED;
  if (alwaysTranslate == _translateInfoBarDelegate->ShouldAlwaysTranslate())
    return;

  _translateInfoBarDelegate->ToggleAlwaysTranslate();
}

@end

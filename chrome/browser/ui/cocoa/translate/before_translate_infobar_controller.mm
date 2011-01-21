// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/translate/before_translate_infobar_controller.h"

#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using TranslateInfoBarUtilities::MoveControl;
using TranslateInfoBarUtilities::VerifyControlOrderAndSpacing;

namespace {

NSButton* CreateNSButtonWithResourceIDAndParameter(
    int resourceId, const string16& param) {
  string16 title = l10n_util::GetStringFUTF16(resourceId, param);
  NSButton* button = [[NSButton alloc] init];
  [button setTitle:base::SysUTF16ToNSString(title)];
  [button setBezelStyle:NSTexturedRoundedBezelStyle];
  return button;
}

} // namespace

@implementation BeforeTranslateInfobarController

- (id) initWithDelegate:(InfoBarDelegate *)delegate {
  if ((self = [super initWithDelegate:delegate])) {
    [self initializeExtraControls];
  }
  return self;
}

- (void)initializeExtraControls {
  TranslateInfoBarDelegate* delegate = [self delegate];
  const string16& language = delegate->GetLanguageDisplayableNameAt(
      delegate->original_language_index());
  neverTranslateButton_.reset(
      CreateNSButtonWithResourceIDAndParameter(
          IDS_TRANSLATE_INFOBAR_NEVER_TRANSLATE, language));
  [neverTranslateButton_ setTarget:self];
  [neverTranslateButton_ setAction:@selector(neverTranslate:)];

  alwaysTranslateButton_.reset(
      CreateNSButtonWithResourceIDAndParameter(
          IDS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE, language));
  [alwaysTranslateButton_ setTarget:self];
  [alwaysTranslateButton_ setAction:@selector(alwaysTranslate:)];
}

- (void)layout {
  MoveControl(label1_, fromLanguagePopUp_, spaceBetweenControls_ / 2, true);
  MoveControl(fromLanguagePopUp_, label2_, spaceBetweenControls_, true);
  MoveControl(label2_, okButton_, spaceBetweenControls_, true);
  MoveControl(okButton_, cancelButton_, spaceBetweenControls_, true);
  NSView* lastControl = cancelButton_;
  if (neverTranslateButton_.get()) {
    MoveControl(lastControl, neverTranslateButton_.get(),
                spaceBetweenControls_, true);
    lastControl = neverTranslateButton_.get();
  }
  if (alwaysTranslateButton_.get()) {
    MoveControl(lastControl, alwaysTranslateButton_.get(),
                spaceBetweenControls_, true);
  }
}

- (void)loadLabelText {
  size_t offset = 0;
  string16 text =
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_BEFORE_MESSAGE,
                                 string16(), &offset);
  NSString* string1 = base::SysUTF16ToNSString(text.substr(0, offset));
  NSString* string2 = base::SysUTF16ToNSString(text.substr(offset));
  [label1_ setStringValue:string1];
  [label2_ setStringValue:string2];
  [label3_ setStringValue:@""];
}

- (NSArray*)visibleControls {
  NSMutableArray* visibleControls = [NSMutableArray arrayWithObjects:
      label1_.get(), fromLanguagePopUp_.get(), label2_.get(),
      okButton_, cancelButton_, nil];

  if ([self delegate]->ShouldShowNeverTranslateButton())
    [visibleControls addObject:neverTranslateButton_.get()];

  if ([self delegate]->ShouldShowAlwaysTranslateButton())
    [visibleControls addObject:alwaysTranslateButton_.get()];

  return visibleControls;
}

// This is called when the "Never Translate [language]" button is pressed.
- (void)neverTranslate:(id)sender {
  [self delegate]->NeverTranslatePageLanguage();
}

// This is called when the "Always Translate [language]" button is pressed.
- (void)alwaysTranslate:(id)sender {
  [self delegate]->AlwaysTranslatePageLanguage();
}

- (bool)verifyLayout {
  if ([optionsPopUp_ isHidden])
    return false;
  return [super verifyLayout];
}

@end

@implementation BeforeTranslateInfobarController (TestingAPI)

- (NSButton*)alwaysTranslateButton {
  return alwaysTranslateButton_.get();
}
- (NSButton*)neverTranslateButton {
  return neverTranslateButton_.get();
}

@end

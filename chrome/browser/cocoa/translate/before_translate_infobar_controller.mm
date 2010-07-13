// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/translate/before_translate_infobar_controller.h"

#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"

using TranslateInfoBarUtilities::MoveControl;
using TranslateInfoBarUtilities::VerifyControlOrderAndSpacing;

@implementation BeforeTranslateInfobarController

- (void)layout {
  MoveControl(label1_, fromLanguagePopUp_, spaceBetweenControls_ / 2, true);
  MoveControl(fromLanguagePopUp_, label2_, spaceBetweenControls_, true);
  MoveControl(label2_, okButton_, spaceBetweenControls_, true);
  MoveControl(okButton_, cancelButton_, spaceBetweenControls_, true);
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
  return [NSArray arrayWithObjects:label1_.get(), fromLanguagePopUp_.get(),
      label2_.get(), okButton_, cancelButton_, nil];
}

- (bool)verifyLayout {
  if ([optionsPopUp_ isHidden])
    return false;
  return [super verifyLayout];
}

@end

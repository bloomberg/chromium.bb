// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/translate/translate_message_infobar_controller.h"

#include "base/sys_string_conversions.h"

using TranslateInfoBarUtilities::MoveControl;

@implementation TranslateMessageInfobarController

- (id)initWithDelegate:(InfoBarDelegate*)delegate {
  if ((self = [super initWithDelegate:delegate])) {
    TranslateInfoBarDelegate* delegate = [self delegate];
    if (delegate->IsError())
      state_ = TranslateInfoBarDelegate::TRANSLATION_ERROR;
    else
      state_ = TranslateInfoBarDelegate::TRANSLATING;
  }
  return self;
}

- (void)layout {
  [optionsPopUp_ setHidden:YES];
  [self removeOkCancelButtons];
  MoveControl(label1_, tryAgainButton_, spaceBetweenControls_ * 2, true);
  TranslateInfoBarDelegate* delegate = [self delegate];
  if (delegate->IsError())
    MoveControl(label1_, tryAgainButton_, spaceBetweenControls_ * 2, true);
}

- (void)adjustOptionsButtonSizeAndVisibilityForView:(NSView*)lastView {
  // Do nothing, but stop the options button from showing up.
}

- (NSArray*)visibleControls {
  NSMutableArray* visibleControls =
      [NSMutableArray arrayWithObjects:label1_.get(), nil];
  if (state_ == TranslateInfoBarDelegate::TRANSLATION_ERROR)
    [visibleControls addObject:tryAgainButton_];
  return visibleControls;
}

- (void)loadLabelText {
  TranslateInfoBarDelegate* delegate = [self delegate];
  string16 messageText = delegate->GetMessageInfoBarText();
  NSString* string1 = base::SysUTF16ToNSString(messageText);
  [label1_ setStringValue:string1];
}

- (bool)verifyLayout {
  if (![optionsPopUp_ isHidden])
    return false;
  return [super verifyLayout];
}

@end

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/translate/translate_message_infobar_controller.h"

#include "base/sys_string_conversions.h"

using TranslateInfoBarUtilities::MoveControl;

@implementation TranslateMessageInfobarController

- (void)layout {
  [self removeOkCancelButtons];
  MoveControl(label1_, translateMessageButton_, spaceBetweenControls_ * 2, true);
  TranslateInfoBarDelegate* delegate = [self delegate];
  if ([self delegate]->ShouldShowMessageInfoBarButton()) {
    string16 buttonText = delegate->GetMessageInfoBarButtonText();
    [translateMessageButton_ setTitle:base::SysUTF16ToNSString(buttonText)];
    [translateMessageButton_ sizeToFit];
  }
}

- (void)adjustOptionsButtonSizeAndVisibilityForView:(NSView*)lastView {
  // Do nothing, but stop the options button from showing up.
}

- (NSArray*)visibleControls {
  NSMutableArray* visibleControls =
      [NSMutableArray arrayWithObjects:label1_.get(), nil];
  if ([self delegate]->ShouldShowMessageInfoBarButton())
    [visibleControls addObject:translateMessageButton_];
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

- (BOOL)shouldShowOptionsPopUp {
  return NO;
}

@end

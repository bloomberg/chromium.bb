// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_text_field_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/credit_card.h"

@implementation AutoFillTextField

- (void)awakeFromNib {
  if ([self tag] == AUTOFILL_CC_TAG)
    isCreditCardField_ = YES;
}

// Override NSResponder method for when the text field may gain focus.  We
// call |scrollRectToVisible| to ensure that this text field is visible within
// the scrolling area.
- (BOOL)becomeFirstResponder {
  // Vertical inset is negative to indicate "outset".  Provides some visual
  // space above and below when tabbing between fields.
  const CGFloat kVerticalInset = -40.0;
  BOOL becoming = [super becomeFirstResponder];
  if (becoming) {
    [self scrollRectToVisible:NSInsetRect([self bounds], 0.0, kVerticalInset)];
  }
  return becoming;
}

- (void)setObjectValue:(id)object {
  if (isCreditCardField_ && [object isKindOfClass:[NSString class]]) {
    // Obfuscate the number.
    NSString* string = object;
    CreditCard card;
    card.SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
                 base::SysNSStringToUTF16(string));
    NSString* starredString = base::SysUTF16ToNSString(card.ObfuscatedNumber());

    [super setObjectValue:starredString];
    isObfuscated_ = YES;
    obfuscatedValue_.reset([string copy]);
  } else {
    [super setObjectValue:object];
  }
}

- (id)objectValue {
  if (isObfuscated_) {
    // This should not happen. This field is bound, and its value will only be
    // fetched if it is changed, and since we force selection, that should clear
    // the obfuscation. Nevertheless, we'll be paranoid here since we don't want
    // the obfuscating ***s to end up in the database.
    return obfuscatedValue_.get();
  } else {
    return [super objectValue];
  }
}

// |self| is automatically set to be the delegate of the field editor; this
// method is called by the field editor.
- (void)textViewDidChangeSelection:(NSNotification *)notification {
  if (isCreditCardField_ && !isBeingSelected_ && isObfuscated_) {
    // Can't edit obfuscated credit card info; force a select-all in that case.
    isBeingSelected_ = YES;
    NSText* editor = [notification object];
    [editor selectAll:self];
    isBeingSelected_ = NO;
  }
}

// Docs aren't clear, but this is called on the first keypress, not when the
// field takes focus.
- (BOOL)textShouldBeginEditing:(NSText*)textObject {
  BOOL should = [super textShouldBeginEditing:textObject];
  // On editing, since everything is selected, the field is now clear.
  isObfuscated_ = !should;
  if (!isObfuscated_)
    obfuscatedValue_.reset();
  return should;
}

@end

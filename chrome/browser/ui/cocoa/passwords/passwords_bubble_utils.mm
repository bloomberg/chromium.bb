// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"
#include "components/autofill/core/common/password_form.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

NSFont* LabelFont() {
  return [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
}

NSSize LabelSize(int resourceID) {
  return [l10n_util::GetNSString(resourceID)
      sizeWithAttributes:@{NSFontAttributeName : LabelFont()}];
}

void InitLabel(NSTextField* textField, const base::string16& text) {
  [textField setStringValue:base::SysUTF16ToNSString(text)];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setFont:LabelFont()];
  [[textField cell] setLineBreakMode:NSLineBreakByTruncatingTail];
  [textField sizeToFit];
}

std::pair<CGFloat, CGFloat> GetResizedColumns(
    CGFloat maxWidth,
    std::pair<CGFloat, CGFloat> columnsWidth) {
  // Free space can be negative.
  CGFloat freeSpace =
      maxWidth - (columnsWidth.first + columnsWidth.second + kItemLabelSpacing);
  if (freeSpace >= 0) {
    return std::make_pair(columnsWidth.first + freeSpace / 2,
                          columnsWidth.second + freeSpace / 2);
  }
  // Make sure that the sizes are nonnegative.
  CGFloat firstColumnPercent =
      columnsWidth.first / (columnsWidth.first + columnsWidth.second);
  return std::make_pair(
      columnsWidth.first + freeSpace * firstColumnPercent,
      columnsWidth.second + freeSpace * (1 - firstColumnPercent));
}

NSSecureTextField* PasswordLabel(const base::string16& text) {
  base::scoped_nsobject<NSSecureTextField> textField(
      [[NSSecureTextField alloc] initWithFrame:NSZeroRect]);
  InitLabel(textField, text);
  return textField.autorelease();
}

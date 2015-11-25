// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_UTILS_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_UTILS_H_

#include <string>

#import <Cocoa/Cocoa.h>

#include "base/strings/string16.h"

namespace autofill {
struct PasswordForm;
}  // autofill

const CGFloat kDesiredBubbleWidth = 370;
const CGFloat kFramePadding = 16;
const CGFloat kItemLabelSpacing = 10;
const CGFloat kRelatedControlHorizontalPadding = 2;
const CGFloat kRelatedControlVerticalSpacing = 8;
const CGFloat kUnrelatedControlVerticalPadding = 15;
const CGFloat kDesiredRowWidth = kDesiredBubbleWidth - 2 * kFramePadding;

// Returns a font for password bubbles.
NSFont* LabelFont();

// Returns a size of a text that is defined by |resourceID| with a font
// returned by LabelFont().
NSSize LabelSize(int resourceID);

// Initialize |textField| with |text| and default style parameters.
void InitLabel(NSTextField* textField, const base::string16& text);

// Returns widths of 2 columns, that are proportional to |columnsWidth| and
// their total width is equal to |maxWidth|.
std::pair<CGFloat, CGFloat> GetResizedColumns(
    CGFloat maxWidth,
    std::pair<CGFloat, CGFloat> columnsWidth);

// Returns a password text field initialized with |text|.
NSSecureTextField* PasswordLabel(const base::string16& text);

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_UTILS_H_

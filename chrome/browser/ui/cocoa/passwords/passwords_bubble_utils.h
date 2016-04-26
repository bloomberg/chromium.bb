// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_UTILS_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_UTILS_H_

#include <string>

#import <Cocoa/Cocoa.h>

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/range/range.h"

@class HyperlinkTextView;

constexpr CGFloat kDesiredBubbleWidth = 370;
constexpr CGFloat kFramePadding = 16;
constexpr CGFloat kDesiredRowWidth = kDesiredBubbleWidth - 2 * kFramePadding;
constexpr CGFloat kItemLabelSpacing = 10;
constexpr CGFloat kRelatedControlHorizontalPadding = 2;
constexpr CGFloat kRelatedControlVerticalSpacing = 8;
constexpr CGFloat kTitleTextInset = 2;
constexpr CGFloat kUnrelatedControlVerticalPadding = 15;
constexpr CGFloat kVerticalAvatarMargin = 8;

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

// Returns a button of the standard style for the bubble.
NSButton* DialogButton(NSString* title);

// Returns a title label with |text| for a bubble. Nonempty |range| may specify
// a link range.
HyperlinkTextView* TitleBubbleLabelWithLink(const base::string16& text,
                                            gfx::Range range,
                                            id<NSTextViewDelegate> delegate);

// Returns a title label with |text| for a dialog. Nonempty |range| may specify
// a link range.
HyperlinkTextView* TitleDialogLabelWithLink(const base::string16& text,
                                            gfx::Range range,
                                            id<NSTextViewDelegate> delegate);

// Returns a label with |text| and small font. Nonempty |range| may specify a
// link range.
HyperlinkTextView* LabelWithLink(const base::string16& text,
                                 SkColor color,
                                 gfx::Range range,
                                 id<NSTextViewDelegate> delegate);

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_BUBBLE_UTILS_H_

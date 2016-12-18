// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_UI_UTIL_H_
#define IOS_CHROME_TODAY_EXTENSION_UI_UTIL_H_

namespace ui_util {

// Inset for the widget.
// This value is only used on iOS10 as the value passed to
// |widgetMarginInsetsForProposedMarginInsets:| is used on previous versions.
extern const CGFloat kDefaultLeadingMarginInset;

// UI metrics to layout the Today extension.
extern const CGFloat kFirstLineHeight;
extern const CGFloat kFirstLineButtonMargin;
extern const CGFloat kSecondLineHeight;
extern const CGFloat kSecondLineVerticalPadding;
extern const CGFloat kUIButtonSeparator;
extern const CGFloat kUIButtonFrontShift;
extern const CGFloat kUIButtonCornerRadius;

extern const CGFloat kFooterVerticalMargin;
extern const CGFloat kFooterHorizontalMargin;
extern const CGFloat kTitleFontSize;

extern const CGFloat kEmptyLabelYOffset;

// Returns the color for the buttons.
UIColor* TitleColor();
UIColor* BorderColor();
UIColor* InkColor();
UIColor* TextColor();
UIColor* BackgroundColor();

UIColor* NormalTintColor();
UIColor* ActiveTintColor();

// Returns the color for footer text.
UIColor* FooterTextColor();

// Returns the color for empty widget label text.
UIColor* emptyLabelColor();

// Returns whether running on an iPad.
bool IsIPadIdiom();

// Returns whether the current language is right to left.
bool IsRTL();

// Returns the offset of Chrome icon in the title bar.
CGFloat ChromeIconOffset();

// Returns the offset of Chrome title in the title bar.
CGFloat ChromeTextOffset();

// Creates constraints so that |filler| fills entirely |container| and make them
// active.
void ConstrainAllSidesOfViewToView(UIView* container, UIView* filler);

}  // namespace ui_util

#endif  // IOS_CHROME_TODAY_EXTENSION_UI_UTIL_H_

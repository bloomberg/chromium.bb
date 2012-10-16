// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONSTRAINED_WINDOW_CONSTANTS_H_
#define CHROME_BROWSER_UI_CONSTRAINED_WINDOW_CONSTANTS_H_

#include "ui/base/resource/resource_bundle.h"

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowConstants
//
// Style hints shared among platforms.
//
class ConstrainedWindowConstants {
 public:
  static const int kTitleTopPadding = 15; // Padding above the title.
  static const int kHorizontalPadding = 20; // Left and right padding.
  static const int kClientTopPadding = 15; // Padding above the client view.
  static const int kClientBottomPadding = 20; // Padding below the client view.
  static const int kCloseButtonPadding = 7; // Padding around the close button.
  static const int kBorderRadius = 2; // Border radius for dialog corners.
  static const int kRowPadding = 20; // Padding between rows of text.

  // Font style for dialog text.
  static const ui::ResourceBundle::FontStyle kTextFontStyle =
      ui::ResourceBundle::BaseFont;
  // Font style for bold dialog text.
  static const ui::ResourceBundle::FontStyle kBoldTextFontStyle =
      ui::ResourceBundle::BoldFont;
  // Font style for dialog title.
  static const ui::ResourceBundle::FontStyle kTitleFontStyle =
      ui::ResourceBundle::MediumFont;
};

#endif  // CHROME_BROWSER_UI_CONSTRAINED_WINDOW_CONSTANTS_H_

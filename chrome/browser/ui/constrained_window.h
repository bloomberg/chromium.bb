// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONSTRAINED_WINDOW_H_
#define CHROME_BROWSER_UI_CONSTRAINED_WINDOW_H_

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/native_widget_types.h"

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindow
//
//  This interface represents a window that is constrained to a
//  WebContentsView's bounds.
//
class ConstrainedWindow {
 public:
  static const int kVerticalPadding = 14; // top/bottom padding.
  static const int kHorizontalPadding = 17; // left/right padding.
  static const int kRowPadding = 20;  // Vertical margin between dialog rows.
  static const int kBorderRadius = 2;  // Border radius for dialog corners.

  // Font style for dialog text.
  static const ui::ResourceBundle::FontStyle kTextFontStyle =
      ui::ResourceBundle::BaseFont;
  // Font style for dialog title.
  static const ui::ResourceBundle::FontStyle kTitleFontStyle =
      ui::ResourceBundle::MediumFont;

  static SkColor GetBackgroundColor();  // Dialog background color.
  static SkColor GetTextColor();  // Dialog text color.

  // Makes the Constrained Window visible. Only one Constrained Window is shown
  // at a time per tab.
  virtual void ShowConstrainedWindow() = 0;

  // Closes the Constrained Window.
  virtual void CloseConstrainedWindow() = 0;

  // Sets focus on the Constrained Window.
  virtual void FocusConstrainedWindow();

  // Checks if the constrained window can be shown.
  virtual bool CanShowConstrainedWindow();

  // Returns the native window of the constrained window.
  virtual gfx::NativeWindow GetNativeWindow();

 protected:
  virtual ~ConstrainedWindow() {}
};

#endif  // CHROME_BROWSER_UI_CONSTRAINED_WINDOW_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "ui/views/view.h"

namespace views {
class ProgressBar;
}  // namespace views

class SkBitmap;

namespace chromeos {

// SettingLevelBubbleView displays information about the current value of a
// level-based setting like volume or brightness.
class SettingLevelBubbleView : public views::View {
 public:
  // Layout() is called before Init(), make sure |progress_bar_| is ready.
  SettingLevelBubbleView();

  // Initialize the view, setting the progress bar to the specified level in the
  // range [0.0, 100.0] and state.  Ownership of |icon| remains with the caller
  // (it's probably a shared instance from ResourceBundle).
  void Init(SkBitmap* icon, double level, bool enabled);

  // Change the icon that we're currently displaying.
  void SetIcon(SkBitmap* icon);

  // Set the progress bar to the specified level and redraw it.
  void SetLevel(double level);

  // Draw the progress bar in an enabled or disabled state.
  void SetEnabled(bool enabled);

  // views::View implementation:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(SettingLevelBubbleTest, CreateAndUpdate);

  views::ProgressBar* progress_bar_;
  SkBitmap* icon_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(SettingLevelBubbleView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_VIEW_H_

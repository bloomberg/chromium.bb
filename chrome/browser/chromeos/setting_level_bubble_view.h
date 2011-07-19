// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_VIEW_H_
#pragma once

#include "views/view.h"

namespace views {
class ProgressBar;
}  // namespace views

class SkBitmap;

namespace chromeos {

// SettingLevelBubbleView displays information about the current value of a
// level-based setting like volume or brightness.
class SettingLevelBubbleView : public views::View {
 public:
  SettingLevelBubbleView();

  // Initialize the view, setting the progress bar to the specified position.
  // Ownership of |icon| remains with the caller (it's probably a shared
  // instance from ResourceBundle).
  void Init(SkBitmap* icon, int level_percent);

  // Change the icon that we're currently displaying.
  void SetIcon(SkBitmap* icon);

  // Set the progress bar to the specified position and redraw it.
  void Update(int level_percent);

  // views::View implementation:
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 private:
  views::ProgressBar* progress_bar_;
  SkBitmap* icon_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(SettingLevelBubbleView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_VIEW_H_

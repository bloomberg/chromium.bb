// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_VIEW_H_

#include "views/view.h"

namespace views {
class ProgressBar;
}  // namespace views

class SkBitmap;

namespace chromeos {

class VolumeBubbleView : public views::View {
 public:
  VolumeBubbleView();

  // Initialize the view. Set the volume progress bar to the specified position.
  void Init(int volume_level_percent);

  // Set the volume progress bar to the specified position and redraw it.
  void Update(int volume_level_percent);

  // views::View implementation:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 private:
  views::ProgressBar* progress_bar_;
  SkBitmap* volume_icon_;

  DISALLOW_COPY_AND_ASSIGN(VolumeBubbleView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_VIEW_H_

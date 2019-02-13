// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_NEXT_TRACK_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_NEXT_TRACK_IMAGE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/views/controls/button/image_button.h"

namespace views {

// A resizable next track image button.
class NextTrackImageButton : public views::ImageButton {
 public:
  explicit NextTrackImageButton(ButtonListener*);
  ~NextTrackImageButton() override;

  // Get button size when visible.
  gfx::Size GetLastVisibleSize() const;

  // Toggle visibility.
  void ToggleVisibility(bool is_visible);

 protected:
  // Overridden from views::View.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  // Last visible size of the image button.
  gfx::Size last_visible_size_;

  DISALLOW_COPY_AND_ASSIGN(NextTrackImageButton);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_NEXT_TRACK_IMAGE_BUTTON_H_

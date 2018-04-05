// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_

#include "chrome/browser/overlay/overlay_window.h"

#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

// The Views implementation of OverlayWindow.
class OverlayWindowViews : public OverlayWindow, public views::Widget {
 public:
  OverlayWindowViews();
  ~OverlayWindowViews() override;

  // OverlayWindow:
  bool IsActive() const override;
  void Show() override;
  void Close() override;
  bool IsVisible() const override;
  bool IsAlwaysOnTop() const override;
  ui::Layer* GetLayer() override;
  gfx::Rect GetBounds() const override;

  // views::Widget:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnNativeWidgetWorkspaceChanged() override;

 private:
  // Determine the intended bounds of |this|. This should be called when there
  // is reason for the bounds to change, such as switching primary displays or
  // playing a new video (i.e. different aspect ratio). This also updates
  // |min_size_| and |max_size_|.
  gfx::Rect CalculateAndUpdateBounds();

  // The upper and lower bounds of |current_size_|. These are determined by the
  // size of the primary display work area when Picture-in-Picture is initiated.
  // TODO(apacible): Update these bounds when the display the window is on
  // changes. http://crbug.com/819673
  gfx::Size min_size_;
  gfx::Size max_size_;

  // Current size of the Picture-in-Picture window.
  gfx::Size current_size_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_

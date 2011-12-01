// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "ui/views/accessible_pane_view.h"

namespace gfx {
class Canvas;
}  // namespace gfx

////////////////////////////////////////////////////////////////////////////////
//
// The DropdownBarView is an abstract view to draw the UI controls of the
// DropdownBarHost.
//
////////////////////////////////////////////////////////////////////////////////
class DropdownBarView : public views::AccessiblePaneView,
                        public DropdownBarHostDelegate {
 public:
  explicit DropdownBarView(DropdownBarHost* host);
  virtual ~DropdownBarView();

  // Updates the view to let it know where the host is clipping the
  // dropdown widget (while animating the opening or closing of the widget).
  virtual void SetAnimationOffset(int offset) OVERRIDE;

  // Returns the offset used while animating.
  int animation_offset() const { return animation_offset_; }

 protected:
  // Returns the DropdownBarHost that manages this view.
  DropdownBarHost* host() const { return host_; }

  // Assign border bitmaps for this drop down instance.
  void SetDialogBorderBitmaps(const SkBitmap* dialog_left,
                              const SkBitmap* dialog_middle,
                              const SkBitmap* dialog_right);

  // Paints the offset toolbar background over the widget area and returns the
  // bounds of the widget.
  gfx::Rect PaintOffsetToolbarBackground(gfx::Canvas* canvas);

  // Paint the border for the drop down dialog given the ends and middle
  // bitmaps.
  void PaintDialogBorder(gfx::Canvas* canvas, const gfx::Rect& bounds) const;

  // Special paint code for the edges when the drop down bar is animating.
  void PaintAnimatingEdges(gfx::Canvas* canvas, const gfx::Rect& bounds) const;

  // The dialog border bitmaps.
  const SkBitmap* dialog_left_;
  const SkBitmap* dialog_middle_;
  const SkBitmap* dialog_right_;

 private:
  // The dropdown bar host that controls this view.
  DropdownBarHost* host_;

  // While animating, the host clips the widget and draws only the bottom
  // part of it. The view needs to know the pixel offset at which we are drawing
  // the widget so that we can draw the curved edges that attach to the toolbar
  // in the right location.
  int animation_offset_;

  DISALLOW_COPY_AND_ASSIGN(DropdownBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_

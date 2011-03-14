// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGED_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGED_TAB_VIEW_H_
#pragma once

#include <vector>

#include "build/build_config.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
#include "views/view.h"

namespace views {
#if defined(OS_WIN)
class WidgetWin;
#elif defined(OS_LINUX)
class WidgetGtk;
#endif
}
namespace gfx {
class Point;
}
class NativeViewPhotobooth;
class Tab;
class TabRenderer;

class DraggedTabView : public views::View {
 public:
  // Creates a new DraggedTabView using |renderers| as the Views. DraggedTabView
  // takes ownership of the views in |renderers|.
  DraggedTabView(const std::vector<views::View*>& renderers,
                 const gfx::Point& mouse_tab_offset,
                 const gfx::Size& contents_size,
                 const gfx::Size& min_size);
  virtual ~DraggedTabView();

  // Moves the DraggedTabView to the appropriate location given the mouse
  // pointer at |screen_point|.
  void MoveTo(const gfx::Point& screen_point);

  // Sets the offset of the mouse from the upper left corner of the tab.
  void set_mouse_tab_offset(const gfx::Point& offset) {
    mouse_tab_offset_ = offset;
  }

  // Sets the width of the dragged tab and updates the dragged image.
  void SetTabWidthAndUpdate(int width, NativeViewPhotobooth* photobooth);

  // Notifies the DraggedTabView that it should update itself.
  void Update();

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // Paint the view, when it's not attached to any TabStrip.
  void PaintDetachedView(gfx::Canvas* canvas);

  // Paint the view, when "Show window contents while dragging" is disabled.
  void PaintFocusRect(gfx::Canvas* canvas);

  // Resizes the container to fit the content for the current attachment mode.
  void ResizeContainer();

  // Utility for scaling a size by the current scaling factor.
  int ScaleValue(int value);

  // The window that contains the DraggedTabView.
#if defined(OS_WIN)
  scoped_ptr<views::WidgetWin> container_;
#elif defined(OS_LINUX)
  scoped_ptr<views::WidgetGtk> container_;
#endif

  // The renderer that paints the Tab shape.
  std::vector<views::View*> renderers_;

  // True if "Show window contents while dragging" is enabled.
  bool show_contents_on_drag_;

  // The unscaled offset of the mouse from the top left of the dragged Tab.
  // This is used to maintain an appropriate offset for the mouse pointer when
  // dragging scaled and unscaled representations, and also to calculate the
  // position of detached windows.
  gfx::Point mouse_tab_offset_;

  // The size of the tab renderer.
  gfx::Size tab_size_;

  // A handle to the DIB containing the current screenshot of the TabContents
  // we are dragging.
  NativeViewPhotobooth* photobooth_;

  // Size of the TabContents being dragged.
  gfx::Size contents_size_;

  DISALLOW_COPY_AND_ASSIGN(DraggedTabView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGED_TAB_VIEW_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TABS_DRAGGED_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_TABS_DRAGGED_VIEW_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <vector>

#include "base/callback_old.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class DragData;
class TabContents;
class TabRendererGtk;

class DraggedViewGtk : public ui::AnimationDelegate {
 public:
  DraggedViewGtk(DragData* drag_data,
                const gfx::Point& mouse_tab_offset,
                const gfx::Size& contents_size);
  virtual ~DraggedViewGtk();

  // Moves the attached dragged view to the appropriate location.
  // |tabstrip_point| is the location of the upper left corner of the dragged
  // view in screen coordinates.
  void MoveAttachedTo(const gfx::Point& tabstrip_point);

  // Moves the detached dragged view to the appropriate location. |screen_point|
  // is the current position of the mouse pointer in screen coordinates.
  void MoveDetachedTo(const gfx::Point& screen_point);

  // Sets the offset of the mouse from the upper left corner of the tab.
  void set_mouse_tab_offset(const gfx::Point& offset) {
    mouse_tab_offset_ = offset;
  }

  // Notifies the dragged tab that it has become attached to a tabstrip.
  // |normal_width| and |mini_width| is the width of a mini and a normal tab
  // respectively after attaching. |parent_window_width| is the width of the
  // parent window of the tabstrip.
  void Attach(int normal_width, int mini_width, int parent_window_width);

  // Resizes the dragged tab to a width of |width|.
  void Resize(int width);

  // Notifies the dragged tab that it has been detached from a tabstrip.
  void Detach();

  // Notifies the dragged tab that it should update itself.
  void Update();

  // Animates the dragged tab to the specified bounds, then calls back to
  // |callback|.
  void AnimateToBounds(const gfx::Rect& bounds, const base::Closure& callback);

  // Returns the size of the dragged tab. Used when attaching to a tabstrip
  // to determine where to place the tab in the attached tabstrip.
  const gfx::Size& attached_tab_size() const { return attached_tab_size_; }
  int GetAttachedTabWidthAt(int index);

  GtkWidget* widget() const { return container_; }

  int mini_width() { return mini_width_; }
  int normal_width() { return normal_width_; }

  // Returns the width occupied in the tabstrip from index |from| included to
  // index |to| excluded. The indices are with respect to |drag_data_|.
  int GetWidthInTabStripFromTo(int from, int to);

  // Returns the total width occupied in the tabstrip.
  int GetTotalWidthInTabStrip();

  // Returns the width occupied in the tabstrip from the left most point of the
  // dragged view up to the source tab excluded.
  int GetWidthInTabStripUpToSourceTab();

  // Returns the width occupied in the tabstrip from the left most point
  // (regardless of RTL or LTR mode) of the dragged view up to the mouse pointer
  // when the drag was initiated.
  int GetWidthInTabStripUpToMousePointer();

  // Returns the distance from the start of the tabstrip (left, regardless of
  // RTL) up to the position of the mouse pointer.
  gfx::Point GetDistanceFromTabStripOriginToMousePointer();

 private:
  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

  // Arranges the contents of the dragged tab.
  void Layout();

  // Gets the preferred size of the dragged tab.
  gfx::Size GetPreferredSize();

  // Resizes the container to fit the content for the current attachment mode.
  void ResizeContainer();

  // Utility for scaling a size by the current scaling factor.
  int ScaleValue(int value);

  // Returns the bounds of the container window.
  gfx::Rect bounds() const;

  // Sets the color map of the container window to allow the window to be
  // transparent.
  void SetContainerColorMap();

  // Sets full transparency for the container window.  This is used if
  // compositing is available for the screen.
  void SetContainerTransparency();

  // Sets the shape mask for the container window to emulate a transparent
  // container window.  This is used if compositing is not available for the
  // screen.
  // |surface| represents the tab only (not the render view).
  void SetContainerShapeMask();

  void PaintTab(int index, GtkWidget* widget, cairo_t* cr, int widget_width);

  // expose-event handler that notifies when the tab needs to be redrawn.
  CHROMEGTK_CALLBACK_1(DraggedViewGtk, gboolean, OnExpose, GdkEventExpose*);

  // The window that contains the dragged tab or tab contents.
  GtkWidget* container_;

  // The fixed widget that we use to contain the tab renderer so that the
  // tab widget won't be resized.
  GtkWidget* fixed_;

  // The renderer that paints the dragged tab.
  std::vector<TabRendererGtk*> renderers_;

  // Holds various data for each dragged tab needed to handle dragging. It is
  // owned by |DraggedTabControllerGtk| class.
  DragData* drag_data_;

  // The width of a mini tab at the time the dragging was initiated.
  int mini_width_;

  // The width of a normal tab (not mini) at the time the dragging was
  // initiated.
  int normal_width_;

  // True if the view is currently attached to a tabstrip. Controls rendering
  // and sizing modes.
  bool attached_;

  // The width of the browser window where the dragged tabs were attached for
  // the last time.
  int parent_window_width_;

  // The unscaled offset of the mouse from the top left of the dragged tab.
  // This is used to maintain an appropriate offset for the mouse pointer when
  // dragging scaled and unscaled representations, and also to calculate the
  // position of detached windows.
  gfx::Point mouse_tab_offset_;

  // The size of the tab renderer when the dragged tab is attached to a
  // tabstrip.
  gfx::Size attached_tab_size_;

  // The dimensions of the TabContents being dragged.
  gfx::Size contents_size_;

  // The animation used to slide the attached tab to its final location.
  ui::SlideAnimation close_animation_;

  // A callback notified when the animation is complete.
  base::Closure animation_callback_;

  // The start and end bounds of the animation sequence.
  gfx::Rect animation_start_bounds_;
  gfx::Rect animation_end_bounds_;

  DISALLOW_COPY_AND_ASSIGN(DraggedViewGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_TABS_DRAGGED_VIEW_GTK_H_

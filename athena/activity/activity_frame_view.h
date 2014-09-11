// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_ACTIVITY_FRAME_VIEW_H_
#define ATHENA_ACTIVITY_ACTIVITY_FRAME_VIEW_H_

#include "athena/wm/public/window_manager_observer.h"
#include "ui/gfx/insets.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class ImageView;
class Label;
class Widget;
}

namespace athena {

class ActivityViewModel;

// A NonClientFrameView used for activity.
class ActivityFrameView : public views::NonClientFrameView,
                          public WindowManagerObserver {
 public:
  // The frame class name.
  static const char kViewClassName[];

  ActivityFrameView(views::Widget* frame, ActivityViewModel* view_model);
  virtual ~ActivityFrameView();

  // views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;
  virtual void SizeConstraintsChanged() OVERRIDE;

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

 private:
  // WindowManagerObserver:
  virtual void OnOverviewModeEnter() OVERRIDE;
  virtual void OnOverviewModeExit() OVERRIDE;
  virtual void OnSplitViewModeEnter() OVERRIDE;
  virtual void OnSplitViewModeExit() OVERRIDE;

  gfx::Insets NonClientBorderInsets() const;
  int NonClientBorderThickness() const;
  int NonClientTopBorderHeight() const;

  // Not owned.
  views::Widget* frame_;
  ActivityViewModel* view_model_;
  views::Label* title_;
  views::ImageView* icon_;

  // Whether overview mode is active.
  bool in_overview_;

  DISALLOW_COPY_AND_ASSIGN(ActivityFrameView);
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_ACTIVITY_FRAME_VIEW_H_

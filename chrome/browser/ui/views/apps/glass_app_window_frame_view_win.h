// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_GLASS_APP_WINDOW_FRAME_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_GLASS_APP_WINDOW_FRAME_VIEW_WIN_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/views/window/non_client_view.h"

namespace extensions {
class NativeAppWindow;
}

// A glass style app window frame view.
class GlassAppWindowFrameViewWin : public views::NonClientFrameView {
 public:
  static const char kViewClassName[];

  explicit GlassAppWindowFrameViewWin(extensions::NativeAppWindow* window,
                                      views::Widget* widget);
  virtual ~GlassAppWindowFrameViewWin();

  gfx::Insets GetGlassInsets() const;

 private:
  // views::NonClientFrameView implementation.
  virtual gfx::Rect GetBoundsForClientView() const override;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  virtual int NonClientHitTest(const gfx::Point& point) override;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) override;
  virtual void ResetWindowControls() override {}
  virtual void UpdateWindowIcon() override {}
  virtual void UpdateWindowTitle() override {}
  virtual void SizeConstraintsChanged() override {}

  // views::View implementation.
  virtual gfx::Size GetPreferredSize() const override;
  virtual const char* GetClassName() const override;
  virtual gfx::Size GetMinimumSize() const override;
  virtual gfx::Size GetMaximumSize() const override;

  extensions::NativeAppWindow* window_;
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(GlassAppWindowFrameViewWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_GLASS_APP_WINDOW_FRAME_VIEW_WIN_H_

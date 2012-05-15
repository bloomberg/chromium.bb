// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "ui/views/controls/button/button.h"  // ButtonListener

namespace ash {
class FramePainter;
}
namespace views {
class ImageButton;
}

class BrowserNonClientFrameViewAsh : public BrowserNonClientFrameView,
                                     public views::ButtonListener,
                                     public TabIconView::TabIconViewModel {
 public:
  static const char kViewClassName[];

  BrowserNonClientFrameViewAsh(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~BrowserNonClientFrameViewAsh();

  void Init();

  // BrowserNonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const OVERRIDE;
  virtual int GetHorizontalTabStripVerticalOffset(
      bool force_restored) const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;

  // views::NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // views::View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from TabIconView::TabIconViewModel:
  virtual bool ShouldTabIconViewAnimate() const OVERRIDE;
  virtual SkBitmap GetFaviconForTabIconView() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest, UseShortHeader);

  // Distance between top of window and client area.
  int NonClientTopBorderHeight(bool force_restored) const;

  // Returns true if we should use a short header, such as for popup windows.
  bool UseShortHeader() const;

  // Layout the incognito icon.
  void LayoutAvatar();

  void PaintTitleBar(gfx::Canvas* canvas);
  void PaintToolbarBackground(gfx::Canvas* canvas);

  // Windows without a toolbar need to draw their own line under the header,
  // above the content area.
  void PaintContentEdge(gfx::Canvas* canvas);

  // Returns the correct bitmap id for the frame header based on activation
  // state and incognito mode.
  int GetThemeFrameBitmapId() const;
  const SkBitmap* GetThemeFrameOverlayBitmap() const;

  // Window controls. The |size_button_| either toggles maximized or toggles
  // minimized. The exact behavior is determined by |size_button_minimizes_|.
  views::ImageButton* size_button_;
  views::ImageButton* close_button_;

  // For popups, the window icon.
  TabIconView* window_icon_;

  // Painter for the frame header.
  scoped_ptr<ash::FramePainter> frame_painter_;

  // If true the |size_button_| minimizes, otherwise it toggles between
  // maximized and restored.
  bool size_button_minimizes_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_

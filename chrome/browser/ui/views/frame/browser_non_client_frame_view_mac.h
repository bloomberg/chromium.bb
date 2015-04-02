// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

class BrowserNonClientFrameViewMac : public BrowserNonClientFrameView {
 public:
  // Mac implementation of BrowserNonClientFrameView.
  BrowserNonClientFrameViewMac(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameViewMac() override;

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const override;
  int GetTopInset() const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // views::View:
  gfx::Size GetMinimumSize() const override;

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // BrowserNonClientFrameView:
  void UpdateNewAvatarButtonImpl() override;

 private:
  void PaintThemedFrame(gfx::Canvas* canvas);
  void PaintToolbarBackground(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewMac);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_

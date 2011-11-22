// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_AURA_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "ui/aura_shell/window_frame.h"
#include "ui/views/widget/widget.h"
#include "views/controls/button/button.h"

class BrowserFrame;
class BrowserView;
class FrameBackground;
class WindowControlButton;

class BrowserNonClientFrameViewAura : public BrowserNonClientFrameView,
                                      public views::ButtonListener,
                                      public views::Widget::Observer,
                                      public aura_shell::WindowFrame {
 public:
  BrowserNonClientFrameViewAura(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~BrowserNonClientFrameViewAura();

  // Control the slide-in animation of the frame background.
  void ShowFrameBackground();
  void HideFrameBackground();

 private:
  // Returns a HitTest code.
  int NonClientHitTestImpl(const gfx::Point& point);

  // Returns the target rectangle for the frame background, based on a mouse
  // position from |hittest_code| and the window's active/inactive state.
  // Pass HTNOWHERE to get default bounds.
  gfx::Rect GetFrameBackgroundBounds(int hittest_code, bool active_window);

  // Recomputes the bounds of the semi-transparent frame background.
  void UpdateFrameBackground(bool active_window);

  // Invoked when the active state changes.
  void ActiveStateChanged();

  // BrowserNonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const OVERRIDE;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;

  // views::NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void ShouldPaintAsActiveChanged() OVERRIDE;

  // views::View overrides:
  virtual void Layout() OVERRIDE;
  virtual views::View* GetEventHandlerForPoint(
      const gfx::Point& point) OVERRIDE;
  virtual bool HitTest(const gfx::Point& p) const OVERRIDE;
  virtual void OnMouseMoved(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const views::MouseEvent& event) OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::Widget::Observer overrides:
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // aura_shell::WindowFrame overrides:
  virtual void OnWindowHoverChanged(bool hovered) OVERRIDE;

  int last_hittest_code_;
  WindowControlButton* maximize_button_;
  WindowControlButton* close_button_;
  FrameBackground* frame_background_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_AURA_H_

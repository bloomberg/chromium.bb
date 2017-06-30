// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_HOST_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "ui/events/event_handler.h"

class BrowserView;
class FullscreenControlView;

namespace ui {
class LocatedEvent;
class GestureEvent;
class MouseEvent;
class TouchEvent;
}  // namespace ui

namespace views {
class View;
}  // namespace views

class FullscreenControlHost : public DropdownBarHost, public ui::EventHandler {
 public:
  // |host_view| allows the host to control the z-order of the underlying view.
  explicit FullscreenControlHost(BrowserView* browser_view,
                                 views::View* host_view);
  ~FullscreenControlHost() override;

  // DropdownBarHost:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;
  gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  FullscreenControlView* fullscreen_control_view() {
    return fullscreen_control_view_;
  }

 private:
  void HandleFullScreenControlVisibility(const ui::LocatedEvent* event);

  BrowserView* const browser_view_;

  // Owned by DropdownBarHost.
  FullscreenControlView* const fullscreen_control_view_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControlHost);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_HOST_H_

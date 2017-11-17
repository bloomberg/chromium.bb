// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_HOST_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_popup.h"
#include "ui/events/event_handler.h"

class BrowserView;
class FullscreenControlView;

namespace ui {
class GestureEvent;
class MouseEvent;
class TouchEvent;
}  // namespace ui

namespace views {
class View;
}  // namespace views

class FullscreenControlHost : public ui::EventHandler {
 public:
  // |host_view| allows the host to control the z-order of the underlying view.
  explicit FullscreenControlHost(BrowserView* browser_view,
                                 views::View* host_view);
  ~FullscreenControlHost() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  void Hide(bool animate);

  bool IsVisible() const;

  FullscreenControlView* fullscreen_control_view() {
    return fullscreen_control_popup_.control_view();
  }

 private:
  // Ensures symmetric input show and hide (e.g. a touch show is hidden by
  // touch).
  enum class InputEntryMethod {
    NOT_ACTIVE,  // The view is hidden.
    MOUSE,       // A mouse event caused the view to show.
    TOUCH,       // A touch event caused the view to show.
  };

  void ShowForInputEntryMethod(InputEntryMethod input_entry_method);
  void OnVisibilityChanged();
  void OnTouchPopupTimeout();

  InputEntryMethod input_entry_method_ = InputEntryMethod::NOT_ACTIVE;

  BrowserView* const browser_view_;

  FullscreenControlPopup fullscreen_control_popup_;
  base::OneShotTimer touch_timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControlHost);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_HOST_H_

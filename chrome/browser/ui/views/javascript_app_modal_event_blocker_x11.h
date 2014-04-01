// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_X11_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_X11_H_

#include "ui/events/event_handler.h"

class BrowserView;

namespace aura {
class Window;
}

// JavascriptAppModalEventBlockerX11 blocks events to all browser windows except
// the browser window which hosts |app_modal_window| for the duration of its
// lifetime. JavascriptAppModalEventBlockerX11 should not outlive
// |app_modal_window|.
// TODO(pkotwicz): Merge this class into WindowModalityController.
class JavascriptAppModalEventBlockerX11 : public ui::EventHandler {
 public:
  explicit JavascriptAppModalEventBlockerX11(aura::Window* app_modal_window);
  virtual ~JavascriptAppModalEventBlockerX11();

 private:
  // Returns true if the propagation of events to |target| should be stopped.
  bool ShouldStopPropagationTo(ui::EventTarget* target);

  // ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // The app modal dialog.
  aura::Window* modal_window_;

  // The BrowserView which hosts the app modal dialog.
  BrowserView* browser_view_with_modal_dialog_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptAppModalEventBlockerX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_X11_H_

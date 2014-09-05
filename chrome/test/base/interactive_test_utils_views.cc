// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/focus/focus_manager.h"

namespace ui_test_utils {

// Until the whole browser UI is ported to tookit-views on Mac, these need to
// use the definitions in interactive_test_utils_mac.mm.
#if !defined(OS_MACOSX)

bool IsViewFocused(const Browser* browser, ViewID vid) {
  BrowserWindow* browser_window = browser->window();
  DCHECK(browser_window);
  gfx::NativeWindow window = browser_window->GetNativeWindow();
  DCHECK(window);
  const views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  const views::FocusManager* focus_manager = widget->GetFocusManager();
  DCHECK(focus_manager);
  DCHECK(focus_manager->GetFocusedView());
  return focus_manager->GetFocusedView()->id() == vid;
}

void ClickOnView(const Browser* browser, ViewID vid) {
  views::View* view =
      BrowserView::GetBrowserViewForBrowser(browser)->GetViewByID(vid);
  DCHECK(view);
  MoveMouseToCenterAndPress(view,
                            ui_controls::LEFT,
                            ui_controls::DOWN | ui_controls::UP,
                            base::MessageLoop::QuitClosure());
  content::RunMessageLoop();
}

void FocusView(const Browser* browser, ViewID vid) {
  views::View* view =
      BrowserView::GetBrowserViewForBrowser(browser)->GetViewByID(vid);
  DCHECK(view);
  view->RequestFocus();
}

#endif  // defined(OS_MACOSX)

void MoveMouseToCenterAndPress(views::View* view,
                               ui_controls::MouseButton button,
                               int state,
                               const base::Closure& closure) {
  DCHECK(view);
  DCHECK(view->GetWidget());
  // Complete any in-progress animation before sending the events so that the
  // mouse-event targetting happens reliably, and does not flake because of
  // unreliable animation state.
  ui::Layer* layer = view->GetWidget()->GetLayer();
  if (layer) {
    ui::LayerAnimator* animator = layer->GetAnimator();
    if (animator && animator->is_animating())
      animator->StopAnimating();
  }

  gfx::Point view_center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &view_center);
  ui_controls::SendMouseMoveNotifyWhenDone(
      view_center.x(),
      view_center.y(),
      base::Bind(&internal::ClickTask, button, state, closure));
}

}  // namespace ui_test_utils

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui_test_utils.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "views/focus/focus_manager.h"

namespace ui_test_utils {

bool IsViewFocused(const Browser* browser, ViewID vid) {
  BrowserWindow* browser_window = browser->window();
  DCHECK(browser_window);
  gfx::NativeWindow window = browser_window->GetNativeHandle();
  DCHECK(window);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(window);
  DCHECK(focus_manager);
  return focus_manager->GetFocusedView()->GetID() == vid;
}

void ClickOnView(const Browser* browser, ViewID vid) {
  BrowserWindow* browser_window = browser->window();
  DCHECK(browser_window);
  views::View* view =
      reinterpret_cast<BrowserView*>(browser_window)->GetViewByID(vid);
  DCHECK(view);
  ui_controls::MoveMouseToCenterAndPress(
      view,
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      new MessageLoop::QuitTask());
  RunMessageLoop();
}

void HideNativeWindow(gfx::NativeWindow window) {
  // TODO(jcampan): retrieve the NativeWidgetWin and show/hide on it instead of
  // using Windows API.
  ::ShowWindow(window, SW_HIDE);
}

void ShowAndFocusNativeWindow(gfx::NativeWindow window) {
  // TODO(jcampan): retrieve the NativeWidgetWin and show/hide on it instead of
  // using Windows API.
  ::ShowWindow(window, SW_SHOW);
}

}  // namespace ui_test_utils

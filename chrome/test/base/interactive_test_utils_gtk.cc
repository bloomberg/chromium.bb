// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/test/base/ui_controls.h"
#include "ui/base/gtk/gtk_screen_util.h"

namespace ui_test_utils {

namespace {
// Check if the focused widget for |root| is |target| or a child of |target|.
static bool IsWidgetInFocusChain(GtkWidget* root, GtkWidget* target) {
  GtkWidget* iter = root;

  while (iter) {
    if (iter == target)
      return true;

    if (!GTK_IS_CONTAINER(iter))
      return false;

    iter = GTK_CONTAINER(iter)->focus_child;
  }

  return false;
}
}  // namespace

bool IsViewFocused(const Browser* browser, ViewID vid) {
  BrowserWindow* browser_window = browser->window();
  DCHECK(browser_window);
  gfx::NativeWindow window = browser_window->GetNativeWindow();
  DCHECK(window);
  GtkWidget* widget = ViewIDUtil::GetWidget(GTK_WIDGET(window), vid);
  DCHECK(widget);
  return IsWidgetInFocusChain(GTK_WIDGET(window), widget);
}

void ClickOnView(const Browser* browser, ViewID vid) {
  BrowserWindow* browser_window = browser->window();
  DCHECK(browser_window);
  gfx::NativeWindow window = browser_window->GetNativeWindow();
  DCHECK(window);
  GtkWidget* view = ViewIDUtil::GetWidget(GTK_WIDGET(window), vid);
  DCHECK(view);
  MoveMouseToCenterAndPress(
      view,
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      MessageLoop::QuitClosure());
  content::RunMessageLoop();
}

void HideNativeWindow(gfx::NativeWindow window) {
  gtk_widget_hide(GTK_WIDGET(window));
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
  if (!gtk_window_has_toplevel_focus(GTK_WINDOW(window)))
    gtk_window_present(GTK_WINDOW(window));
  return true;
}

void MoveMouseToCenterAndPress(GtkWidget* widget,
                               ui_controls::MouseButton button,
                               int state,
                               const base::Closure& task) {
  gfx::Rect bounds = ui::GetWidgetScreenBounds(widget);
  ui_controls::SendMouseMoveNotifyWhenDone(
      bounds.x() + bounds.width() / 2,
      bounds.y() + bounds.height() / 2,
      base::Bind(&internal::ClickTask, button, state, task));
}

}  // namespace ui_test_utils

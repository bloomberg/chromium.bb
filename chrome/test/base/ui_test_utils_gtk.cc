// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/focus/focus_manager.h"
#endif

namespace ui_test_utils {

#if !defined(TOOLKIT_VIEWS)
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
#endif

bool IsViewFocused(const Browser* browser, ViewID vid) {
  BrowserWindow* browser_window = browser->window();
  DCHECK(browser_window);
#if defined(TOOLKIT_VIEWS)
  gfx::NativeWindow window = browser_window->GetNativeHandle();
  const views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  const views::FocusManager* focus_manager = widget->GetFocusManager();
  DCHECK(focus_manager);
  return focus_manager->GetFocusedView() &&
      focus_manager->GetFocusedView()->id() == vid;
#else
  gfx::NativeWindow window = browser_window->GetNativeHandle();
  DCHECK(window);
  GtkWidget* widget = ViewIDUtil::GetWidget(GTK_WIDGET(window), vid);
  DCHECK(widget);
  return IsWidgetInFocusChain(GTK_WIDGET(window), widget);
#endif
}

void ClickOnView(const Browser* browser, ViewID vid) {
  BrowserWindow* browser_window = browser->window();
  DCHECK(browser_window);
#if defined(TOOLKIT_VIEWS)
  views::View* view =
      reinterpret_cast<BrowserView*>(browser_window)->GetViewByID(vid);
#else
  gfx::NativeWindow window = browser_window->GetNativeHandle();
  DCHECK(window);
  GtkWidget* view = ViewIDUtil::GetWidget(GTK_WIDGET(window), vid);
#endif

  DCHECK(view);
  ui_controls::MoveMouseToCenterAndPress(
      view,
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      MessageLoop::QuitClosure());
  RunMessageLoop();
}

void HideNativeWindow(gfx::NativeWindow window) {
  gtk_widget_hide(GTK_WIDGET(window));
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
  if (!gtk_window_has_toplevel_focus(GTK_WINDOW(window)))
    gtk_window_present(GTK_WINDOW(window));
  return true;
}

}  // namespace ui_test_utils

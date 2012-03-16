// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "ui/base/gtk/gtk_screen_util.h"
#include "ui/ui_controls/ui_controls.h"

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
  MoveMouseToCenterAndPress(
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

#if defined(TOOLKIT_VIEWS)

void OnConfigure(GtkWidget* gtk_widget, GdkEvent* event, gpointer data) {
  views::Widget* widget = static_cast<views::Widget*>(data);
  gfx::Rect actual = widget->GetWindowScreenBounds();
  gfx::Rect desired = widget->GetRootView()->bounds();
  if (actual.size() == desired.size())
    MessageLoop::current()->Quit();
}

void SynchronizeWidgetSize(views::Widget* widget) {
  // If the actual window size and desired window size
  // are different, wait until the window is resized
  // to desired size.
  gfx::Rect actual = widget->GetWindowScreenBounds();
  gfx::Rect desired = widget->GetRootView()->bounds();
  if (actual.size() != desired.size()) {
    // Listen to configure-event that is emitted when an window gets
    // resized.
    GtkWidget* gtk_widget = widget->GetNativeView();
    g_signal_connect(gtk_widget, "configure-event",
                     G_CALLBACK(&OnConfigure), widget);
    MessageLoop::current()->Run();
  }
}

void MoveMouseToCenterAndPress(views::View* view,
                               ui_controls::MouseButton button,
                               int state,
                               const base::Closure& task) {
  // X is asynchronous and we need to wait until the window gets
  // resized to desired size.
  SynchronizeWidgetSize(view->GetWidget());

  gfx::Point view_center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &view_center);
  ui_controls::SendMouseMoveNotifyWhenDone(
      view_center.x(), view_center.y(),
      base::Bind(&internal::ClickTask, button, state, task));
}

#else  // !defined(TOOLKIT_VIEWS)

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

#endif  // defined(TOOLKIT_VIEWS

}  // namespace ui_test_utils

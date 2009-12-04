// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui_test_utils.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/automation/ui_controls.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/frame/browser_view.h"
#endif

#include "chrome/browser/gtk/view_id_util.h"

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
  gfx::NativeWindow window = browser_window->GetNativeHandle();
  DCHECK(window);
  GtkWidget* widget = ViewIDUtil::GetWidget(GTK_WIDGET(window), vid);
  DCHECK(widget);
  return IsWidgetInFocusChain(GTK_WIDGET(window), widget);
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
      new MessageLoop::QuitTask());
  RunMessageLoop();
}

}  // namespace ui_test_utils

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/common/automation_messages.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace {

// This function sends a WindowDragResponse message with the appropriate routing
// ID to the automation proxy.
void SendWindowDragResponse(AutomationProvider* provider,
                        IPC::Message* reply_message) {
  AutomationMsg_WindowDrag::WriteReplyParams(reply_message, true);
  provider->Send(reply_message);
}

}  // namespace

void AutomationProvider::PrintAsync(int tab_handle) {
  NOTIMPLEMENTED();
}

void AutomationProvider::WindowSimulateDrag(
    int handle,
    const std::vector<gfx::Point>& drag_path,
    int flags,
    bool press_escape_en_route,
    IPC::Message* reply_message) {
  // TODO(estade): don't ignore |flags| or |escape_en_route|.
  gfx::NativeWindow window =
      browser_tracker_->GetResource(handle)->window()->GetNativeHandle();
  if (window && (drag_path.size() > 1)) {
    int x, y;
    gdk_window_get_position(gtk_widget_get_window(GTK_WIDGET(window)), &x, &y);

    // Create a nested stack of tasks to run.
    base::Closure drag_response_cb = base::Bind(
        &SendWindowDragResponse, make_scoped_refptr(this), reply_message);
    base::Closure move_chain_cb = base::IgnoreReturn<bool>(
        base::Bind(&ui_controls::SendMouseEventsNotifyWhenDone,
                   ui_controls::LEFT, ui_controls::UP, drag_response_cb));
    move_chain_cb = base::IgnoreReturn<bool>(
        base::Bind(&ui_controls::SendMouseEventsNotifyWhenDone,
                   ui_controls::LEFT, ui_controls::UP, move_chain_cb));
    for (size_t i = drag_path.size() - 1; i > 0; --i) {
      // Smooth out the mouse movements by adding intermediate points. This
      // better simulates a real user drag.
      int dest_x = drag_path[i].x() + x;
      int dest_y = drag_path[i].y() + y;
      int half_step_x = (dest_x + drag_path[i - 1].x() + x) / 2;
      int half_step_y = (dest_y + drag_path[i - 1].y() + y) / 2;

      move_chain_cb = base::IgnoreReturn<bool>(
          base::Bind(&ui_controls::SendMouseMoveNotifyWhenDone, dest_x, dest_y,
                     move_chain_cb));
      move_chain_cb = base::IgnoreReturn<bool>(
          base::Bind(&ui_controls::SendMouseMoveNotifyWhenDone, half_step_x,
                         half_step_y, move_chain_cb));
    }
    move_chain_cb = base::IgnoreReturn<bool>(
        base::Bind(&ui_controls::SendMouseEventsNotifyWhenDone,
                   ui_controls::LEFT, ui_controls::DOWN, move_chain_cb));

    ui_controls::SendMouseMoveNotifyWhenDone(x + drag_path[0].x(),
                                             y + drag_path[0].y(),
                                             move_chain_cb);
  } else {
    AutomationMsg_WindowDrag::WriteReplyParams(reply_message, false);
    Send(reply_message);
  }
}

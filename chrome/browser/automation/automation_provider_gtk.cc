// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/common/automation_messages.h"
#include "gfx/point.h"
#include "gfx/rect.h"

void AutomationProvider::PrintAsync(int tab_handle) {
  NOTIMPLEMENTED();
}

// This task sends a WindowDragResponse message with the appropriate
// routing ID to the automation proxy.  This is implemented as a task so that
// we know that the mouse events (and any tasks that they spawn on the message
// loop) have been processed by the time this is sent.
class WindowDragResponseTask : public Task {
 public:
  WindowDragResponseTask(AutomationProvider* provider,
                         IPC::Message* reply_message)
      : provider_(provider),
        reply_message_(reply_message) {
    DCHECK(provider_);
    DCHECK(reply_message_);
  }

  virtual ~WindowDragResponseTask() {
  }

  virtual void Run() {
    AutomationMsg_WindowDrag::WriteReplyParams(reply_message_, true);
    provider_->Send(reply_message_);
  }

 private:
  AutomationProvider* provider_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(WindowDragResponseTask);
};

// A task that just runs a SendMouseEvent and performs another task when done.
class MouseEventTask : public Task {
 public:
  MouseEventTask(Task* next_task, ui_controls::MouseButtonState state)
      : next_task_(next_task),
        state_(state) {}

  virtual ~MouseEventTask() {
  }

  virtual void Run() {
    ui_controls::SendMouseEventsNotifyWhenDone(ui_controls::LEFT, state_,
                                               next_task_);
  }

 private:
  // The task to execute when we are done.
  Task* next_task_;

  // Mouse press or mouse release.
  ui_controls::MouseButtonState state_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventTask);
};

// A task that just runs a SendMouseMove and performs another task when done.
class MouseMoveTask : public Task {
 public:
  MouseMoveTask(Task* next_task, int absolute_x, int absolute_y)
      : next_task_(next_task),
        x_(absolute_x),
        y_(absolute_y) {
  }

  virtual ~MouseMoveTask() {
  }

  virtual void Run() {
    ui_controls::SendMouseMoveNotifyWhenDone(x_, y_, next_task_);
  }

 private:
  // The task to execute when we are done.
  Task* next_task_;

  // Coordinates of the press.
  int x_;
  int y_;

  DISALLOW_COPY_AND_ASSIGN(MouseMoveTask);
};

void AutomationProvider::WindowSimulateDrag(int handle,
                                            std::vector<gfx::Point> drag_path,
                                            int flags,
                                            bool press_escape_en_route,
                                            IPC::Message* reply_message) {
  // TODO(estade): don't ignore |flags| or |escape_en_route|.
  gfx::NativeWindow window =
      browser_tracker_->GetResource(handle)->window()->GetNativeHandle();
  if (window && (drag_path.size() > 1)) {
    int x, y;
    gdk_window_get_position(GTK_WIDGET(window)->window, &x, &y);

    // Create a nested stack of tasks to run.
    Task* next_task = new WindowDragResponseTask(this, reply_message);
    next_task = new MouseEventTask(next_task, ui_controls::UP);
    next_task = new MouseEventTask(next_task, ui_controls::UP);
    for (size_t i = drag_path.size() - 1; i > 0; --i) {
      // Smooth out the mouse movements by adding intermediate points. This
      // better simulates a real user drag.
      int dest_x = drag_path[i].x() + x;
      int dest_y = drag_path[i].y() + y;
      int half_step_x = (dest_x + drag_path[i - 1].x() + x) / 2;
      int half_step_y = (dest_y + drag_path[i - 1].y() + y) / 2;

      next_task = new MouseMoveTask(next_task, dest_x, dest_y);
      next_task = new MouseMoveTask(next_task, half_step_x, half_step_y);
    }
    next_task = new MouseEventTask(next_task, ui_controls::DOWN);

    ui_controls::SendMouseMoveNotifyWhenDone(x + drag_path[0].x(),
                                             y + drag_path[0].y(),
                                             next_task);
  } else {
    AutomationMsg_WindowDrag::WriteReplyParams(reply_message, false);
    Send(reply_message);
  }
}

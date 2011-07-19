// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_
#define CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_

#include "base/task.h"
#include "chrome/browser/automation/ui_controls.h"

namespace ui_controls {

// A utility class to send a mouse click event in a task.
// It's shared by ui_controls_linux.cc and ui_controls_mac.cc
class ClickTask : public Task {
 public:
  // A |followup| Task can be specified to notify the caller when the mouse
  // click event is sent. If can be NULL if the caller does not care about it.
  ClickTask(MouseButton button, int state, Task* followup);
  virtual ~ClickTask();
  virtual void Run();

 private:
  MouseButton button_;
  int state_;
  Task* followup_;
};

}  // namespace ui_controls

#endif  // CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_

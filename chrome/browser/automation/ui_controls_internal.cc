// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls_internal.h"

namespace ui_controls {

ClickTask::ClickTask(MouseButton button, int state, Task* followup)
    : button_(button), state_(state), followup_(followup)  {
}

ClickTask::~ClickTask() {}

void ClickTask::Run() {
  if (followup_)
    SendMouseEventsNotifyWhenDone(button_, state_, followup_);
  else
    SendMouseEvents(button_, state_);
}

}  // ui_controls

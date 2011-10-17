// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_
#define CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_

#include "base/callback.h"
#include "chrome/browser/automation/ui_controls.h"

namespace ui_controls {

// A utility function to send a mouse click event in a closure. It's shared by
// ui_controls_linux.cc and ui_controls_mac.cc
void ClickTask(MouseButton button, int state, const base::Closure& followup);

}  // namespace ui_controls

#endif  // CHROME_BROWSER_AUTOMATION_UI_CONTROLS_INTERNAL_H_

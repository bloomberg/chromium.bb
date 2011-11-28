// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls_internal.h"

#include "base/callback.h"

namespace ui_controls {
namespace internal {

void ClickTask(MouseButton button, int state, const base::Closure& followup) {
  if (!followup.is_null())
    SendMouseEventsNotifyWhenDone(button, state, followup);
  else
    SendMouseEvents(button, state);
}

}  // namespace internal
}  // namespace ui_controls

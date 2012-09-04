// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/event_disposition.h"

#include "chrome/browser/disposition_utils.h"
#include "ui/base/events/event_constants.h"

namespace chrome {

WindowOpenDisposition DispositionFromEventFlags(int event_flags) {
  return disposition_utils::DispositionFromClick(
      (event_flags & ui::EF_MIDDLE_MOUSE_BUTTON) != 0,
      (event_flags & ui::EF_ALT_DOWN) != 0,
      (event_flags & ui::EF_CONTROL_DOWN) != 0,
      (event_flags & ui::EF_COMMAND_DOWN) != 0,
      (event_flags & ui::EF_SHIFT_DOWN) != 0);
}

}  // namespace chrome

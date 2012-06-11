// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/event_utils.h"

#include "ui/views/events/event.h"

namespace event_utils {

bool IsPossibleDispositionEvent(const views::MouseEvent& event) {
  return event.IsLeftMouseButton() || event.IsMiddleMouseButton();
}

}  // namespace event_utils

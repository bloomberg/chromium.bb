// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/event_disposition.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "content/browser/disposition_utils.h"
#include "ui/views/events/event.h"

using views::Event;

namespace event_utils {

// TODO(shinyak) After refactoring, this function shoulbe removed.
WindowOpenDisposition DispositionFromEventFlags(int event_flags) {
  return browser::DispositionFromEventFlags(event_flags);
}

bool IsPossibleDispositionEvent(const views::MouseEvent& event) {
  return event.IsLeftMouseButton() || event.IsMiddleMouseButton();
}

}

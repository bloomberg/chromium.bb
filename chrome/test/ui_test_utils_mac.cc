// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui_test_utils.h"

#include "base/logging.h"

namespace ui_test_utils {

// Details on why these are unimplemented: ViewIDs are defined in
// chrome/browser/view_ids.h.  For Cocoa (unlike Views GTK) we don't
// associate ViewIDs with NSViews.
//
// Here's an idea on how to implement.
// - associate the correct ViewID with an NSView on construction (the most work)
// - create a mapping table, such as chrome/browser/gtk/view_id_util.h
// - IsViewFocused() then becomes
//    [browser->window()->GetNativeHandle() firstResponder]
// - ClickOnView() becomes a normal NSMouseDown event forge at the right coords

bool IsViewFocused(const Browser* browser, ViewID vid) {
  NOTIMPLEMENTED();
  return false;
}

void ClickOnView(const Browser* browser, ViewID vid) {
  NOTIMPLEMENTED();
}

}  // namespace ui_test_utils

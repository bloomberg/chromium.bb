// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_END_DRAG_REASON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_END_DRAG_REASON_H_

// Enum passed to EndDrag().
enum EndDragReason {
  // Complete the drag.
  END_DRAG_COMPLETE,

  // Cancel/revert the drag.
  END_DRAG_CANCEL,

  // The drag should end as the result of a capture lost.
  END_DRAG_CAPTURE_LOST,
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_END_DRAG_REASON_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VALIDATION_MESSAGE_BUBBLE_H_
#define CHROME_BROWSER_UI_VALIDATION_MESSAGE_BUBBLE_H_

#include <memory>

#include "base/strings/string16.h"

namespace content {
class RenderWidgetHost;
}

namespace gfx {
class Rect;
}

class ValidationMessageBubble {
 public:
  // Open a tooltip-like window to show the specified messages.  The window
  // should not change focus state.
  static base::WeakPtr<ValidationMessageBubble> CreateAndShow(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_root_view,
      const base::string16& main_text,
      const base::string16& sub_text);

  // Move the window to a position such that the bubble arrow points to the
  // specified anchor.  |anchor_in_root_view| is in DIP unit, and relative to
  // RWHV for |widget_host|.
  virtual void SetPositionRelativeToAnchor(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_root_view) = 0;

  // Called to close the bubble. This should lead to destruction of |this|.
  virtual void CloseValidationMessage() = 0;

 protected:
  virtual ~ValidationMessageBubble() {}
};

#endif  // CHROME_BROWSER_UI_VALIDATION_MESSAGE_BUBBLE_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_TRACKER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_TRACKER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/web/WebNode.h"

namespace content {
class RenderFrame;
}

namespace autofill {

class PageClickListener;

// This class is responsible notifying the associated listener when a node is
// clicked or tapped. It also tracks whether a node was focused before the event
// was handled.
//
// This is useful for password/form autofill where we want to trigger a
// suggestion popup when a text input is clicked.
//
// There is one PageClickTracker per AutofillAgent.
class PageClickTracker {
 public:
  // The |listener| will be notified when an element is clicked.  It must
  // outlive this class.
  PageClickTracker(content::RenderFrame* render_frame,
                   PageClickListener* listener);
  ~PageClickTracker();

  void FocusedNodeChanged(const blink::WebNode& node);
  void DidCompleteFocusChangeInFrame();
  void DidReceiveLeftMouseDownOrGestureTapInNode(const blink::WebNode& node);

  content::RenderFrame* render_frame() const { return render_frame_; }

 private:
  void DoFocusChangeComplete();

  // True when the last click was on the focused node.
  bool focused_node_was_last_clicked_;

  // This is set to false when the focus changes, then set back to true soon
  // afterwards. This helps track whether an event happened after a node was
  // already focused, or if it caused the focus to change.
  bool was_focused_before_now_;

  // The listener getting the actual notifications.
  PageClickListener* listener_;

  content::RenderFrame* const render_frame_;

  DISALLOW_COPY_AND_ASSIGN(PageClickTracker);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_TRACKER_H_

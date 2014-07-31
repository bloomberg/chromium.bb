// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_TRACKER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_TRACKER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebNode.h"

namespace autofill {

class PageClickListener;

// This class is responsible notifiying the associated listener when a node is
// clicked or tapped. It also tracks whether a node was focused before the event
// was handled.
//
// This is useful for password/form autofill where we want to trigger a
// suggestion popup when a text input is clicked.
//
// There is one PageClickTracker per RenderView.
class PageClickTracker : public content::RenderViewObserver {
 public:
  // The |listener| will be notified when an element is clicked.  It must
  // outlive this class.
  PageClickTracker(content::RenderView* render_view,
                   PageClickListener* listener);
  virtual ~PageClickTracker();

 private:
  // RenderView::Observer implementation.
  virtual void DidHandleMouseEvent(const blink::WebMouseEvent& event) OVERRIDE;
  virtual void DidHandleGestureEvent(
      const blink::WebGestureEvent& event) OVERRIDE;
  virtual void FocusedNodeChanged(const blink::WebNode& node) OVERRIDE;

  // Called there is a tap or click at |x|, |y|.
  void PotentialActivationAt(int x, int y);

  // Sets |was_focused_before_now_| to true.
  void SetWasFocused();

  // This is set to false when the focus changes, then set back to true soon
  // afterwards. This helps track whether an event happened after a node was
  // already focused, or if it caused the focus to change.
  bool was_focused_before_now_;

  // The listener getting the actual notifications.
  PageClickListener* listener_;

  base::WeakPtrFactory<PageClickTracker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageClickTracker);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_TRACKER_H_

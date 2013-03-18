// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNC_INPUT_EVENT_FILTER_H_
#define CONTENT_BROWSER_ANDROID_SYNC_INPUT_EVENT_FILTER_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/port/common/input_event_ack_state.h"

namespace WebKit {
class WebCompositorInputHandler;
class WebInputEvent;
}

namespace content {

// Performs synchronous event filtering, to be used for browser-side,
// in-process event handling when the UI and compositor share threads.
// The filter is associated with at most one view, and one event handler.
class SyncInputEventFilter {
 public:

  SyncInputEventFilter();
  ~SyncInputEventFilter();

  InputEventAckState HandleInputEvent(const WebKit::WebInputEvent& input_event);
  void SetInputHandler(WebKit::WebCompositorInputHandler* input_handler);
  void ClearInputHandler();

 private:
  // Used to DCHECK that input_handler_ changes are made on the correct thread.
  base::ThreadChecker thread_checker_;

  class InputHandlerWrapper;
  scoped_ptr<InputHandlerWrapper> input_handler_;

  DISALLOW_COPY_AND_ASSIGN(SyncInputEventFilter);
};

}  // namespace content

#endif // CONTENT_BROWSER_ANDROID_SYNC_INPUT_EVENT_FILTER_H_

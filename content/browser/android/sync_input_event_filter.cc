// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "content/browser/android/sync_input_event_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebInputEvent;

namespace content {

//------------------------------------------------------------------------------

SyncInputEventFilter::SyncInputEventFilter() {
}

SyncInputEventFilter::~SyncInputEventFilter() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

InputEventAckState SyncInputEventFilter::HandleInputEvent(
    const WebInputEvent& event) {
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void SyncInputEventFilter::ClearInputHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

}  // namespace content

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "content/browser/android/sync_input_event_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositorInputHandler.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositorInputHandlerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebInputEvent;

namespace content {

class SyncInputEventFilter::InputHandlerWrapper
    : public WebKit::WebCompositorInputHandlerClient {
 public:
  InputHandlerWrapper(SyncInputEventFilter* event_filter,
                      WebKit::WebCompositorInputHandler* input_handler)
      : event_result_(INPUT_EVENT_ACK_STATE_UNKNOWN),
        event_filter_(event_filter),
        input_handler_(input_handler) {
    input_handler_->setClient(this);
  }

  virtual ~InputHandlerWrapper() {
    input_handler_->setClient(NULL);
  }

  InputEventAckState HandleInputEvent(
      const WebKit::WebInputEvent& input_event) {

    // Clear the result for the (unexpected) case that callbacks to
    // did/didNotHandleInputEvent are not made synchronously.
    event_result_ = INPUT_EVENT_ACK_STATE_UNKNOWN;

    // It is expected that input_handler_ makes an appropriate synchronous
    // callback to did/didNotHandleInputEvent. event_result_ is then assigned in
    // those callbacks.
    input_handler_->handleInputEvent(input_event);

    DCHECK(event_result_ != INPUT_EVENT_ACK_STATE_UNKNOWN);

    return event_result_;
  }

  WebKit::WebCompositorInputHandler* input_handler() const {
    return input_handler_;
  }

  // WebCompositorInputHandlerClient methods:

  virtual void willShutdown() {
    event_filter_->ClearInputHandler();
  }

  virtual void didHandleInputEvent() {
    event_result_ = INPUT_EVENT_ACK_STATE_CONSUMED;
  }

  virtual void didNotHandleInputEvent(bool send_to_widget) {
    event_result_ = send_to_widget ? INPUT_EVENT_ACK_STATE_NOT_CONSUMED
                                   : INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }

 private:
  // This acts as a temporary result, storing the result of the
  // did/didNotHandleInputEvent callbacks.  We use it to avoid creating a
  // closure or reference to a stack variable.
  InputEventAckState event_result_;

  SyncInputEventFilter* event_filter_;
  WebKit::WebCompositorInputHandler* input_handler_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerWrapper);
};

//------------------------------------------------------------------------------

SyncInputEventFilter::SyncInputEventFilter() {
}

SyncInputEventFilter::~SyncInputEventFilter() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

InputEventAckState SyncInputEventFilter::HandleInputEvent(
    const WebInputEvent& event) {
  if (!input_handler_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  return input_handler_->HandleInputEvent(event);
}

void SyncInputEventFilter::SetInputHandler(
    WebKit::WebCompositorInputHandler* new_input_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!new_input_handler) {
    return;
  }

  if (input_handler_) {
    // It's valid to call SetInputHandler() with the same input_handler many
    // times, but it's not valid to change the input_handler once it's been set.
    DCHECK_EQ(input_handler_->input_handler(), new_input_handler);
    return;
  }

  TRACE_EVENT0("SyncInputEventFilter::SetInputHandler",
               "SettingHandler");
  input_handler_.reset(new InputHandlerWrapper(this, new_input_handler));
}

void SyncInputEventFilter::ClearInputHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("SyncInputEventFilter::ClearInputHandler",
               "ClearingHandler");
  input_handler_.reset();
}

}  // namespace content

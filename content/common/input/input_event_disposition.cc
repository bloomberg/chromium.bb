// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_event_disposition.h"

#include "base/logging.h"

namespace content {

InputEventDisposition ToDisposition(InputEventAckState ack_state,
                                    bool main_thread,
                                    bool prevent_default) {
  if (main_thread) {
    switch (ack_state) {
      case INPUT_EVENT_ACK_STATE_CONSUMED:
        return INPUT_EVENT_MAIN_THREAD_CONSUMED;
      case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
        return prevent_default ? INPUT_EVENT_MAIN_THREAD_PREVENT_DEFAULTED
                               : INPUT_EVENT_MAIN_THREAD_NOT_PREVENT_DEFAULTED;
      case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
        return INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS;
      case INPUT_EVENT_ACK_STATE_UNKNOWN:
        return INPUT_EVENT_UNHANDLED;
    }
  } else {
    switch (ack_state) {
      case INPUT_EVENT_ACK_STATE_CONSUMED:
        return INPUT_EVENT_IMPL_THREAD_CONSUMED;
      case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
        return INPUT_EVENT_IMPL_THREAD_BOUNCE_TO_MAIN;
      case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
        return INPUT_EVENT_IMPL_THREAD_NO_CONSUMER_EXISTS;
      case INPUT_EVENT_ACK_STATE_UNKNOWN:
        return INPUT_EVENT_UNHANDLED;
    }
  }
  NOTREACHED();
  return INPUT_EVENT_UNHANDLED;
}

InputEventAckState ToAckState(InputEventDisposition disposition) {
  switch (disposition) {
    case INPUT_EVENT_UNHANDLED:
      return INPUT_EVENT_ACK_STATE_UNKNOWN;
    case INPUT_EVENT_IMPL_THREAD_CONSUMED:
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    case INPUT_EVENT_IMPL_THREAD_NO_CONSUMER_EXISTS:
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    case INPUT_EVENT_IMPL_THREAD_BOUNCE_TO_MAIN:
      return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    case INPUT_EVENT_COULD_NOT_DELIVER:
      return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    case INPUT_EVENT_MAIN_THREAD_CONSUMED:
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    case INPUT_EVENT_MAIN_THREAD_PREVENT_DEFAULTED:
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    case INPUT_EVENT_MAIN_THREAD_NOT_PREVENT_DEFAULTED:
      return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    case INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS:
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }
  NOTREACHED();
  return INPUT_EVENT_ACK_STATE_UNKNOWN;
}

}  // namespace content

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_EVENT_DISPOSITION_H_
#define CONTENT_COMMON_INPUT_INPUT_EVENT_DISPOSITION_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/port/common/input_event_ack_state.h"

namespace content {

// Disposition of input events as they are handled and passed between the
// browser and renderer.
enum InputEventDisposition {
  INPUT_EVENT_UNHANDLED,
  INPUT_EVENT_COULD_NOT_DELIVER,

  INPUT_EVENT_IMPL_THREAD_CONSUMED,
  INPUT_EVENT_IMPL_THREAD_BOUNCE_TO_MAIN,
  INPUT_EVENT_IMPL_THREAD_NO_CONSUMER_EXISTS,

  INPUT_EVENT_MAIN_THREAD_CONSUMED,
  INPUT_EVENT_MAIN_THREAD_PREVENT_DEFAULTED,
  INPUT_EVENT_MAIN_THREAD_NOT_PREVENT_DEFAULTED,
  INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS,

  INPUT_EVENT_DISPOSITION_MAX = INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS
};
typedef std::vector<InputEventDisposition> InputEventDispositions;

InputEventDisposition CONTENT_EXPORT ToDisposition(InputEventAckState ack,
                                                   bool main_thread,
                                                   bool prevent_default);
InputEventAckState CONTENT_EXPORT ToAckState(InputEventDisposition disposition);

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_INPUT_EVENT_DISPOSITION_H_

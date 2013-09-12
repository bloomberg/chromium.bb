// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_QUEUE_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_QUEUE_CLIENT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "content/common/input/input_event_disposition.h"

namespace content {

class EventPacket;

class InputQueueClient {
 public:
  virtual ~InputQueueClient() {}

  // Used by the flush process for sending batched events to the desired target.
  virtual void Deliver(const EventPacket& packet) = 0;

  // Signalled when the InputQueue has received and dispatched
  // event ACK's from the previous flush.
  virtual void DidFinishFlush() = 0;

  // Signalled when the InputQueue receives an input event.
  virtual void SetNeedsFlush() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_QUEUE_CLIENT_H_

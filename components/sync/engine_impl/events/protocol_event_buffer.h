// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_PROTOCOL_EVENT_BUFFER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_PROTOCOL_EVENT_BUFFER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"

namespace syncer {

class ProtocolEvent;

// A container for ProtocolEvents.
//
// Stores at most kBufferSize events, then starts dropping the oldest events.
class ProtocolEventBuffer {
 public:
  static const size_t kBufferSize;

  ProtocolEventBuffer();
  ~ProtocolEventBuffer();

  // Records an event.  May cause the oldest event to be dropped.
  void RecordProtocolEvent(const ProtocolEvent& event);

  // Returns copies of the buffered contents.  Will not clear the buffer.
  std::vector<std::unique_ptr<ProtocolEvent>> GetBufferedProtocolEvents() const;

 private:
  base::circular_deque<std::unique_ptr<ProtocolEvent>> buffer_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolEventBuffer);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_PROTOCOL_EVENT_BUFFER_H_

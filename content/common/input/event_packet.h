// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_EVENT_PACKET_H_
#define CONTENT_COMMON_INPUT_EVENT_PACKET_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"

namespace content {

class InputEvent;

// An id'ed sequence of InputEvents.
class CONTENT_EXPORT EventPacket {
 public:
  typedef ScopedVector<InputEvent> InputEvents;

  EventPacket();
  ~EventPacket();

  // Only a non-NULL and valid |event| will be accepted and added to the packet.
  // Returns true if |event| was added, and false otherwise.
  bool Add(scoped_ptr<InputEvent> event);

  void set_id(int64 id) { id_ = id; }
  int64 id() const { return id_; }

  // Accessor functions for convenience.
  bool empty() const { return events_.empty(); }
  size_t size() const { return events_.size(); }
  InputEvents::const_iterator begin() const { return events_.end(); }
  InputEvents::const_iterator end() const { return events_.end(); }
  const InputEvents& events() const { return events_; }

 protected:
  int64 id_;
  InputEvents events_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventPacket);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_EVENT_PACKET_H_

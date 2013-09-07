// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/event_packet.h"

#include "content/common/input/input_event.h"

namespace content {

EventPacket::EventPacket() : id_(0) {}

EventPacket::~EventPacket() {}

bool EventPacket::Add(scoped_ptr<InputEvent> event) {
  if (!event || !event->valid())
    return false;
  events_.push_back(event.release());
  return true;
}

}  // namespace content

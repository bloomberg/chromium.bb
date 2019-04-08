// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/port.h"

namespace mojo {
namespace core {
namespace ports {

Port::Port(uint64_t next_sequence_num_to_send,
           uint64_t next_sequence_num_to_receive)
    : state(kUninitialized),
      next_sequence_num_to_send(next_sequence_num_to_send),
      last_sequence_num_to_receive(0),
      message_queue(next_sequence_num_to_receive),
      remove_proxy_on_last_message(false),
      peer_closed(false) {}

Port::~Port() {}

Port::Slot* Port::GetSlot(SlotId slot_id) {
  auto it = slots.find(slot_id);
  if (it == slots.end())
    return nullptr;
  return &it->second;
}

SlotId Port::AllocateSlot() {
  DCHECK_EQ(state, kReceiving);
  SlotId id = ++last_allocated_slot_id;
  CHECK_EQ(id & kPeerAllocatedSlotIdBit, 0u);
  slots.emplace(id, Slot{});
  return id;
}

bool Port::AddSlotFromPeer(SlotId peer_slot_id) {
  if (state != kReceiving || (peer_slot_id & kPeerAllocatedSlotIdBit) != 0)
    return false;
  auto result = slots.emplace(peer_slot_id | kPeerAllocatedSlotIdBit, Slot{});
  result.first->second.can_signal = true;
  return result.second;
}

Port::Slot::Slot() = default;

Port::Slot::Slot(const Slot&) = default;

Port::Slot::~Slot() = default;

}  // namespace ports
}  // namespace core
}  // namespace mojo

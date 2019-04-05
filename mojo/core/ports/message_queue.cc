// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/message_queue.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/core/ports/message_filter.h"

namespace mojo {
namespace core {
namespace ports {

// Used by std::{push,pop}_heap functions
inline bool operator<(const MessageQueue::Entry& a,
                      const MessageQueue::Entry& b) {
  return a.message->sequence_num() > b.message->sequence_num();
}

MessageQueue::MessageQueue() : MessageQueue(kInitialSequenceNum) {}

MessageQueue::MessageQueue(uint64_t next_sequence_num)
    : next_sequence_num_(next_sequence_num) {
  // The message queue is blocked waiting for a message with sequence number
  // equal to |next_sequence_num|.
}

MessageQueue::~MessageQueue() {
#if DCHECK_IS_ON()
  size_t num_leaked_ports = 0;
  for (const auto& entry : heap_)
    num_leaked_ports += entry.message->num_ports();
  DVLOG_IF(1, num_leaked_ports > 0)
      << "Leaking " << num_leaked_ports << " ports in unreceived messages";
#endif
}

bool MessageQueue::HasNextMessage(base::Optional<SlotId> slot_id) {
  DropNextIgnoredMessages();
  return !heap_.empty() &&
         heap_[0].message->sequence_num() == next_sequence_num_ &&
         (!slot_id || heap_[0].message->slot_id() == slot_id);
}

base::Optional<SlotId> MessageQueue::GetNextMessageSlot() {
  DropNextIgnoredMessages();
  if (heap_.empty() || heap_[0].message->sequence_num() != next_sequence_num_)
    return base::nullopt;
  return heap_[0].message->slot_id();
}

void MessageQueue::GetNextMessage(base::Optional<SlotId> slot_id,
                                  std::unique_ptr<UserMessageEvent>* message,
                                  MessageFilter* filter) {
  if (!HasNextMessage(slot_id) ||
      (filter && !filter->Match(*heap_[0].message))) {
    message->reset();
    return;
  }

  *message = DequeueMessage();
}

void MessageQueue::AcceptMessage(
    std::unique_ptr<UserMessageEvent> message,
    base::Optional<SlotId>* slot_with_next_message) {
  // TODO: Handle sequence number roll-over.

  EnqueueMessage(std::move(message), false /* ignored */);
  if (heap_[0].message->sequence_num() == next_sequence_num_)
    *slot_with_next_message = heap_[0].message->slot_id();
  else
    slot_with_next_message->reset();
}

void MessageQueue::IgnoreMessage(std::unique_ptr<UserMessageEvent>* message) {
  if ((*message)->sequence_num() == next_sequence_num_) {
    // Fast path -- if we're ignoring the next message in the sequence, just
    // advance the queue's sequence number.
    next_sequence_num_++;
    return;
  }

  EnqueueMessage(std::move(*message), true /* ignored */);
}

void MessageQueue::TakeAllMessages(
    std::vector<std::unique_ptr<UserMessageEvent>>* messages) {
  for (auto& entry : heap_)
    messages->emplace_back(std::move(entry.message));
  heap_.clear();
  total_queued_bytes_ = 0;
}

void MessageQueue::TakeAllLeadingMessagesForSlot(
    SlotId slot_id,
    std::vector<std::unique_ptr<UserMessageEvent>>* messages) {
  std::unique_ptr<UserMessageEvent> message;
  for (;;) {
    GetNextMessage(slot_id, &message, nullptr);
    if (!message)
      return;
    messages->push_back(std::move(message));
  }
}

void MessageQueue::EnqueueMessage(std::unique_ptr<UserMessageEvent> message,
                                  bool ignored) {
  total_queued_bytes_ += message->GetSizeIfSerialized();
  heap_.emplace_back(ignored, std::move(message));
  std::push_heap(heap_.begin(), heap_.end());
}

std::unique_ptr<UserMessageEvent> MessageQueue::DequeueMessage() {
  std::pop_heap(heap_.begin(), heap_.end());
  auto message = std::move(heap_.back().message);
  total_queued_bytes_ -= message->GetSizeIfSerialized();
  heap_.pop_back();

  // We keep the capacity of |heap_| in check so that a large batch of incoming
  // messages doesn't permanently wreck available memory. The choice of interval
  // here is somewhat arbitrary.
  constexpr size_t kHeapMinimumShrinkSize = 16;
  constexpr size_t kHeapShrinkInterval = 512;
  if (UNLIKELY(heap_.size() > kHeapMinimumShrinkSize &&
               heap_.size() % kHeapShrinkInterval == 0)) {
    heap_.shrink_to_fit();
  }

  next_sequence_num_++;

  return message;
}

void MessageQueue::DropNextIgnoredMessages() {
  while (!heap_.empty() &&
         heap_[0].message->sequence_num() == next_sequence_num_ &&
         heap_[0].ignored) {
    DequeueMessage();
  }
}

MessageQueue::Entry::Entry(bool ignored,
                           std::unique_ptr<UserMessageEvent> message)
    : ignored(ignored), message(std::move(message)) {}

MessageQueue::Entry::Entry(Entry&&) = default;

MessageQueue::Entry::~Entry() = default;

MessageQueue::Entry& MessageQueue::Entry::operator=(Entry&&) = default;

}  // namespace ports
}  // namespace core
}  // namespace mojo

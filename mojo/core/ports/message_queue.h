// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_MESSAGE_QUEUE_H_
#define MOJO_CORE_PORTS_MESSAGE_QUEUE_H_

#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/core/ports/event.h"

namespace mojo {
namespace core {
namespace ports {

constexpr uint64_t kInitialSequenceNum = 1;
constexpr uint64_t kInvalidSequenceNum = std::numeric_limits<uint64_t>::max();

class MessageFilter;

// An incoming message queue for a port. MessageQueue keeps track of the highest
// known sequence number and can indicate whether the next sequential message is
// available. Thus the queue enforces message ordering for the consumer without
// enforcing it for the producer (see AcceptMessage() below.)
class COMPONENT_EXPORT(MOJO_CORE_PORTS) MessageQueue {
 public:
  struct Entry {
    Entry(bool ignored, std::unique_ptr<UserMessageEvent> message);
    Entry(Entry&&);
    ~Entry();

    Entry& operator=(Entry&&);

    bool ignored;
    std::unique_ptr<UserMessageEvent> message;
  };

  explicit MessageQueue();
  explicit MessageQueue(uint64_t next_sequence_num);
  ~MessageQueue();

  uint64_t next_sequence_num() const { return next_sequence_num_; }

  // Indicates if the next message in the queue's sequence is available and
  // is targeting slot |slot_id|, if given.
  bool HasNextMessage(base::Optional<SlotId> slot_id);

  // Indicates if the next message in the queue's sequence is available,
  // regardless of targeted slot ID. If it is, this returns the SlotId of the
  // next available message. Otherwise it returns |base::nullopt|.
  base::Optional<SlotId> GetNextMessageSlot();

  // Gives ownership of the message. If |filter| is non-null, the next message
  // will only be retrieved if the filter successfully matches it. If |slot_id|
  // is given, this next message will only be retrieved if it belongs to that
  // slot.
  void GetNextMessage(base::Optional<SlotId> slot_id,
                      std::unique_ptr<UserMessageEvent>* message,
                      MessageFilter* filter);

  // Takes ownership of the message. Note: Messages are ordered, so while we
  // have added a message to the queue, we may still be waiting on a message
  // ahead of this one before we can let any of the messages be returned by
  // GetNextMessage.
  //
  // |*slot_with_next_message| is set to the SlotId of the slot which has the
  // next message in the sequence available to it, iff the next message in the
  // sequence is available.
  void AcceptMessage(std::unique_ptr<UserMessageEvent> message,
                     base::Optional<SlotId>* slot_with_next_message);

  // Places |*message| in the queue to be ignored. It is important that the
  // message not simply be discarded because messages can arrive out of
  // sequential order. The queue will retain this message until all preceding
  // messages in the sequence are read from the queue, at which point |*message|
  // can be safely discarded and the sequence can be advanced beyond it.
  //
  // Note that if |*message| is the next expected message in this queue, the
  // queue is simply advanced beyond its sequence number and ownership of
  // |*message| is *not* transferred to the queue.
  void IgnoreMessage(std::unique_ptr<UserMessageEvent>* message);

  // Takes all messages from this queue. Used to safely destroy queued messages
  // without holding any Port lock.
  void TakeAllMessages(
      std::vector<std::unique_ptr<UserMessageEvent>>* messages);

  // Takes all leading messages from the queue which target |slot_id|.
  void TakeAllLeadingMessagesForSlot(
      SlotId slot_id,
      std::vector<std::unique_ptr<UserMessageEvent>>* messages);

  // The number of messages queued here, regardless of whether the next expected
  // message has arrived yet.
  size_t queued_message_count() const { return heap_.size(); }

  // The aggregate memory size in bytes of all messages queued here, regardless
  // of whether the next expected message has arrived yet.
  size_t queued_num_bytes() const { return total_queued_bytes_; }

 private:
  void EnqueueMessage(std::unique_ptr<UserMessageEvent> message, bool ignored);
  std::unique_ptr<UserMessageEvent> DequeueMessage();
  void DropNextIgnoredMessages();

  std::vector<Entry> heap_;
  uint64_t next_sequence_num_;
  size_t total_queued_bytes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_MESSAGE_QUEUE_H_

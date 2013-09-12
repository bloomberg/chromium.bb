// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_queue.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "content/browser/renderer_host/input/browser_input_event.h"
#include "content/browser/renderer_host/input/input_queue_client.h"
#include "content/common/input/event_packet.h"
#include "content/common/input/input_event.h"

namespace content {

// A specialized EventPacket with utility methods for dispatched event handling.
class InputQueue::BrowserEventPacket : public EventPacket {
 public:
  typedef ScopedVector<BrowserInputEvent> BrowserInputEvents;

  BrowserEventPacket() : weak_factory_(this) {}
  virtual ~BrowserEventPacket() {}

  // Validate the response and signal dispatch to the processed events.
  // Undelivered events will be re-enqueued, and any generated followup events
  // will be inserted at the same relative order as their generating event.
  AckResult ValidateAndDispatchAck(int64 packet_id,
                                   const InputEventDispositions& dispositions) {
    if (!Validate(packet_id, dispositions))
      return ACK_INVALID;

    // Empty the packet; events will be re-enqueued as necessary.
    InputEvents dispatched_events;
    events_.swap(dispatched_events);

    // The packet could be deleted as a result of event dispatch; use a local
    // weak ref to ensure proper shutdown.
    base::WeakPtr<EventPacket> weak_ref_this = weak_factory_.GetWeakPtr();

    BrowserInputEvents followup_events;
    for (size_t i = 0; i < dispatched_events.size(); ++i) {
      // Take ownership of the event.
      scoped_ptr<BrowserInputEvent> event(
          static_cast<BrowserInputEvent*>(dispatched_events[i]));
      dispatched_events[i] = NULL;

      // Re-enqueue undelivered events.
      InputEventDisposition disposition = dispositions[i];
      if (disposition == INPUT_EVENT_COULD_NOT_DELIVER) {
        Add(event.PassAs<InputEvent>());
        continue;
      }

      event->OnDispatched(disposition, &followup_events);

      // TODO(jdduke): http://crbug.com/274029
      if (!weak_ref_this.get())
        return ACK_SHUTDOWN;

      AddAll(&followup_events);
    }
    return ACK_OK;
  }

 protected:
  // Add and take ownership of events in |followup_events|.
  void AddAll(BrowserInputEvents* followup_events) {
    for (BrowserInputEvents::iterator iter = followup_events->begin();
         iter != followup_events->end();
         ++iter) {
      Add(scoped_ptr<InputEvent>(*iter));
    }
    followup_events->weak_clear();
  }

  // Perform a sanity check of the ack against the current packet.
  // |packet_id| should match that of this packet, and |dispositions| should
  // be of size equal to the number of events in this packet.
  bool Validate(int64 packet_id,
                const InputEventDispositions& dispositions) const {
    if (packet_id != id())
      return false;

    if (dispositions.size() != size())
      return false;

    return true;
  }

 private:
  base::WeakPtrFactory<EventPacket> weak_factory_;
};

InputQueue::InputQueue(InputQueueClient* client)
    : client_(client),
      next_packet_id_(1),
      flush_requested_(false),
      in_flush_packet_(new BrowserEventPacket()),
      pending_flush_packet_(new BrowserEventPacket()) {
  DCHECK(client_);
}

InputQueue::~InputQueue() {}

void InputQueue::QueueEvent(scoped_ptr<BrowserInputEvent> event) {
  DCHECK(event);
  DCHECK(event->valid());
  pending_flush_packet_->Add(event.PassAs<InputEvent>());
  RequestFlushIfNecessary();
}

void InputQueue::BeginFlush() {
  // Ignore repeated flush attempts.
  if (!flush_requested_)
    return;

  DCHECK(!FlushInProgress());
  DCHECK(!pending_flush_packet_->empty());

  flush_requested_ = false;
  in_flush_packet_.swap(pending_flush_packet_);
  DeliverInFlushPacket();
}

InputQueue::AckResult InputQueue::OnEventPacketAck(
    int64 packet_id,
    const InputEventDispositions& dispositions) {
  if (!FlushInProgress())
    return ACK_UNEXPECTED;

  TRACE_EVENT_ASYNC_STEP1("input", "InputQueueFlush", this, "AckPacket",
                          "id", packet_id);

  AckResult ack_result =
      in_flush_packet_->ValidateAndDispatchAck(packet_id, dispositions);

  if (ack_result != ACK_OK)
    return ack_result;

  if (FlushInProgress()) {
    DeliverInFlushPacket();
  } else {
    TRACE_EVENT_ASYNC_END0("input", "InputQueueFlush", this);
    client_->DidFinishFlush();
    RequestFlushIfNecessary();
  }

  return ACK_OK;
}

size_t InputQueue::QueuedEventCount() const {
  return in_flush_packet_->size() + pending_flush_packet_->size();
}

void InputQueue::DeliverInFlushPacket() {
  DCHECK(FlushInProgress());
  TRACE_EVENT_ASYNC_STEP1("input", "InputQueueFlush", this, "DeliverPacket",
                          "id", next_packet_id_);
  in_flush_packet_->set_id(next_packet_id_++);
  client_->Deliver(*in_flush_packet_);
}

void InputQueue::RequestFlushIfNecessary() {
  if (flush_requested_)
    return;

  // Defer flush requests until the current flush has finished.
  if (FlushInProgress())
    return;

  // No additional events to flush.
  if (pending_flush_packet_->empty())
    return;

  TRACE_EVENT_ASYNC_BEGIN0("input", "InputQueueFlush", this);
  TRACE_EVENT_ASYNC_STEP0("input", "InputQueueFlush", this, "Request");
  flush_requested_ = true;
  client_->SetNeedsFlush();
}

bool InputQueue::FlushInProgress() const { return !in_flush_packet_->empty(); }

} // namespace content

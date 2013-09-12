// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_QUEUE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_disposition.h"

namespace content {

class BrowserInputEvent;
class EventPacketAck;
class InputQueueClient;

// |InputQueue| handles browser input event batching and response.
// Event packets are assembled into sequential event packets. A flush entails
// delivery and dispatch of a single event packet, and continues until the
// packet is ack'ed and all its events have been dispatched to the renderer.
class CONTENT_EXPORT InputQueue {
 public:
  // The |client| should outlive the InputQueue.
  InputQueue(InputQueueClient* client);
  virtual ~InputQueue();

  // If a flush is in progress, |event| will be dispatched in the next flush.
  // If no flush is in progress, a flush will be requested if necessary.
  // |event| is assumed to be both non-NULL and valid.
  void QueueEvent(scoped_ptr<BrowserInputEvent> event);

  // Initiates the flush of the pending event packet to |client_|, if necessary.
  // This should only be called in response to |client_->SetNeedsFlush()|.
  void BeginFlush();

  // Called by the owner upon EventPacket responses from the sender target. This
  // will dispatch event acks for events with a corresponding |ack_handler|.
  enum AckResult {
    ACK_OK,          // |acked_packet| was processed successfully.
    ACK_UNEXPECTED,  // |acked_packet| was unexpected; no flush was in-progress.
    ACK_INVALID,     // |acked_packet| contains invalid data.
    ACK_SHUTDOWN     // |acked_packet| processing triggered queue shutdown.
  };
  AckResult OnEventPacketAck(int64 packet_id,
                             const InputEventDispositions& dispositions);

  // Total number of evenst in the queue, both being flushed and pending flush.
  size_t QueuedEventCount() const;

 protected:
  friend class InputQueueTest;

  // Delivers |in_flush_packet_| to the client.
  void DeliverInFlushPacket();

  // Requests a flush via |client_| if the necessary request has not been made.
  void RequestFlushIfNecessary();

  // True when |in_flush_packet_| is non-empty.
  bool FlushInProgress() const;

 private:
  InputQueueClient* client_;

  // Used to assign unique, non-zero ID's to each delivered EventPacket.
  int64 next_packet_id_;

  // Avoid spamming the client with redundant flush requests.
  bool flush_requested_;

  class BrowserEventPacket;
  scoped_ptr<BrowserEventPacket> in_flush_packet_;
  scoped_ptr<BrowserEventPacket> pending_flush_packet_;

  DISALLOW_COPY_AND_ASSIGN(InputQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_QUEUE_H_

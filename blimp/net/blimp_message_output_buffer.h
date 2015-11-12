// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_OUTPUT_BUFFER_H_
#define BLIMP_NET_BLIMP_MESSAGE_OUTPUT_BUFFER_H_

#include "base/macros.h"
#include "blimp/net/blimp_message_checkpoint_observer.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class BlimpConnection;

// Provides a FIFO buffer for reliable, ordered message delivery.
// Messages are retained for redelivery until they are acknowledged by the
// receiving end (via BlimpMessageAckObserver).
// (Redelivery will be used in a future CL to implement Fast Recovery
// of dropped connections.)
class BLIMP_NET_EXPORT BlimpMessageOutputBuffer
    : public BlimpMessageProcessor,
      public BlimpMessageCheckpointObserver {
 public:
  BlimpMessageOutputBuffer();
  ~BlimpMessageOutputBuffer() override;

  // Sets the processor to receive messages from this buffer.
  // TODO(kmarshall): implement this.
  void set_output_processor(BlimpMessageProcessor* processor) {}

  // BlimpMessageProcessor implementation.
  // |callback|, if set, will be called once the remote end has acknowledged the
  // receipt of |message|.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  // MessageCheckpointObserver implementation.
  // Flushes acknowledged messages from the buffer and invokes their
  // |callbacks|, if any.
  // Callbacks should not destroy |this| so as to not interfere with the
  // processing of any other pending callbacks.
  void OnMessageCheckpoint(int64 message_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpMessageOutputBuffer);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_OUTPUT_BUFFER_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_checkpointer.h"

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"

namespace blimp {

namespace {
const int kDeferCheckpointAckSeconds = 1;
}

BlimpMessageCheckpointer::BlimpMessageCheckpointer(
    BlimpMessageProcessor* incoming_processor,
    BlimpMessageProcessor* outgoing_processor)
    : incoming_processor_(incoming_processor),
      outgoing_processor_(outgoing_processor),
      weak_factory_(this) {}

BlimpMessageCheckpointer::~BlimpMessageCheckpointer() {}

void BlimpMessageCheckpointer::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  // TODO(wez): Provide independent checkpoints for each message->type()?
  DCHECK(message->has_message_id());

  // Store the message-Id to include in the checkpoint ACK.
  checkpoint_id_ = message->message_id();

  // Kick the timer, if not running, to ACK after a short delay.
  if (!defer_timer_.IsRunning()) {
    defer_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromSeconds(kDeferCheckpointAckSeconds),
                       this, &BlimpMessageCheckpointer::SendCheckpointAck);
  }

  // Pass the message along for actual processing.
  incoming_processor_->ProcessMessage(
      std::move(message),
      base::Bind(&BlimpMessageCheckpointer::InvokeCompletionCallback,
                 weak_factory_.GetWeakPtr(), callback));
}

void BlimpMessageCheckpointer::InvokeCompletionCallback(
    const net::CompletionCallback& callback,
    int result) {
  callback.Run(result);
}

void BlimpMessageCheckpointer::SendCheckpointAck() {
  outgoing_processor_->ProcessMessage(
      CreateCheckpointAckMessage(checkpoint_id_), net::CompletionCallback());
}

}  // namespace blimp

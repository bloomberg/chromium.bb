// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_pump.h"

#include "base/macros.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/packet_reader.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace blimp {

BlimpMessagePump::BlimpMessagePump(PacketReader* reader)
    : reader_(reader),
      error_observer_(nullptr),
      processor_(nullptr),
      buffer_(new net::GrowableIOBuffer) {
  DCHECK(reader_);
}

BlimpMessagePump::~BlimpMessagePump() {}

void BlimpMessagePump::SetMessageProcessor(BlimpMessageProcessor* processor) {
  process_msg_callback_.Cancel();
  processor_ = processor;
  if (!processor_) {
    read_packet_callback_.Cancel();
  } else if (read_packet_callback_.IsCancelled()) {
    buffer_->SetCapacity(kMaxPacketPayloadSizeBytes);
    ReadNextPacket();
  }
}

void BlimpMessagePump::ReadNextPacket() {
  DCHECK(processor_);
  buffer_->set_offset(0);
  read_packet_callback_.Reset(base::Bind(
      &BlimpMessagePump::OnReadPacketComplete, base::Unretained(this)));
  int result =
      reader_->ReadPacket(buffer_.get(), read_packet_callback_.callback());
  if (result != net::ERR_IO_PENDING) {
    // Read completed synchronously.
    OnReadPacketComplete(result);
  }
}

void BlimpMessagePump::OnReadPacketComplete(int result) {
  read_packet_callback_.Cancel();
  if (result > 0) {
    // The result is the size of the packet in bytes.
    scoped_ptr<BlimpMessage> message(new BlimpMessage);
    bool parse_result =
        message->ParseFromArray(buffer_->StartOfBuffer(), result);
    if (parse_result) {
      process_msg_callback_.Reset(base::Bind(
          &BlimpMessagePump::OnProcessMessageComplete, base::Unretained(this)));
      processor_->ProcessMessage(std::move(message),
                                 process_msg_callback_.callback());
      return;
    }
    result = net::ERR_FAILED;
  }
  if (error_observer_)
    error_observer_->OnConnectionError(result);
}

void BlimpMessagePump::OnProcessMessageComplete(int result) {
  // No error is expected from the message receiver.
  DCHECK_EQ(result, net::OK);
  process_msg_callback_.Cancel();
  ReadNextPacket();
}

}  // namespace blimp

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
  buffer_->SetCapacity(kMaxPacketPayloadSizeBytes);
}

BlimpMessagePump::~BlimpMessagePump() {}

void BlimpMessagePump::SetMessageProcessor(BlimpMessageProcessor* processor) {
  DCHECK(!processor_);
  processor_ = processor;
  ReadNextPacket();
}

void BlimpMessagePump::ReadNextPacket() {
  DCHECK(processor_);
  buffer_->set_offset(0);
  read_callback_.Reset(base::Bind(&BlimpMessagePump::OnReadPacketComplete,
                                  base::Unretained(this)));
  reader_->ReadPacket(buffer_.get(), read_callback_.callback());
}

void BlimpMessagePump::OnReadPacketComplete(int result) {
  if (result == net::OK) {
    scoped_ptr<BlimpMessage> message(new BlimpMessage);
    if (message->ParseFromArray(buffer_->StartOfBuffer(), buffer_->offset())) {
      process_msg_callback_.Reset(base::Bind(
          &BlimpMessagePump::OnProcessMessageComplete, base::Unretained(this)));
      processor_->ProcessMessage(std::move(message),
                                 process_msg_callback_.callback());
    } else {
      result = net::ERR_FAILED;
    }
  }

  if (result != net::OK) {
    error_observer_->OnConnectionError(result);
  }
}

void BlimpMessagePump::OnProcessMessageComplete(int result) {
  // No error is expected from the message receiver.
  DCHECK_EQ(net::OK, result);
  ReadNextPacket();
}

}  // namespace blimp

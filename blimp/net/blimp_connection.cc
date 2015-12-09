// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_connection.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_message_pump.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/packet_reader.h"
#include "blimp/net/packet_writer.h"
#include "net/base/completion_callback.h"

namespace blimp {
namespace {

// Forwards incoming blimp messages to PacketWriter.
class BlimpMessageSender : public BlimpMessageProcessor {
 public:
  explicit BlimpMessageSender(PacketWriter* writer);
  ~BlimpMessageSender() override;

  void set_error_observer(ConnectionErrorObserver* observer) {
    error_observer_ = observer;
  }

  // BlimpMessageProcessor implementation.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  void OnWritePacketComplete(int result);

  PacketWriter* writer_;
  ConnectionErrorObserver* error_observer_;
  scoped_refptr<net::DrainableIOBuffer> buffer_;
  net::CompletionCallback pending_process_msg_callback_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageSender);
};

BlimpMessageSender::BlimpMessageSender(PacketWriter* writer)
    : writer_(writer),
      buffer_(new net::DrainableIOBuffer(
          new net::IOBuffer(kMaxPacketPayloadSizeBytes),
          kMaxPacketPayloadSizeBytes)) {
  DCHECK(writer_);
}

BlimpMessageSender::~BlimpMessageSender() {}

void BlimpMessageSender::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  if (message->ByteSize() > static_cast<int>(kMaxPacketPayloadSizeBytes)) {
    DLOG(ERROR) << "Message is too big, size=" << message->ByteSize();
    callback.Run(net::ERR_MSG_TOO_BIG);
    return;
  }

  buffer_->SetOffset(0);
  if (!message->SerializeToArray(buffer_->data(), message->ByteSize())) {
    DLOG(ERROR) << "Failed to serialize message.";
    callback.Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  pending_process_msg_callback_ = callback;
  writer_->WritePacket(buffer_,
                       base::Bind(&BlimpMessageSender::OnWritePacketComplete,
                                  base::Unretained(this)));
}

void BlimpMessageSender::OnWritePacketComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  base::ResetAndReturn(&pending_process_msg_callback_).Run(result);
  if (result != net::OK) {
    error_observer_->OnConnectionError(result);
  }
}

}  // namespace

BlimpConnection::BlimpConnection(scoped_ptr<PacketReader> reader,
                                 scoped_ptr<PacketWriter> writer)
    : reader_(std::move(reader)),
      message_pump_(new BlimpMessagePump(reader_.get())),
      writer_(std::move(writer)),
      outgoing_msg_processor_(new BlimpMessageSender(writer_.get())) {
  DCHECK(writer_);
}

BlimpConnection::BlimpConnection() {}

BlimpConnection::~BlimpConnection() {}

void BlimpConnection::SetConnectionErrorObserver(
    ConnectionErrorObserver* observer) {
  message_pump_->set_error_observer(observer);
  BlimpMessageSender* sender =
      static_cast<BlimpMessageSender*>(outgoing_msg_processor_.get());
  sender->set_error_observer(observer);
}

void BlimpConnection::SetIncomingMessageProcessor(
    BlimpMessageProcessor* processor) {
  message_pump_->SetMessageProcessor(processor);
}

BlimpMessageProcessor* BlimpConnection::GetOutgoingMessageProcessor() const {
  return outgoing_msg_processor_.get();
}

}  // namespace blimp

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/stream_packet_writer.h"

#include <iostream>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/sys_byteorder.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/common.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace blimp {

std::ostream& operator<<(std::ostream& out,
                         const StreamPacketWriter::WriteState state) {
  switch (state) {
    case StreamPacketWriter::WriteState::IDLE:
      out << "IDLE";
      break;
    case StreamPacketWriter::WriteState::HEADER:
      out << "HEADER";
      break;
    case StreamPacketWriter::WriteState::PAYLOAD:
      out << "PAYLOAD";
      break;
  }
  return out;
}

StreamPacketWriter::StreamPacketWriter(net::StreamSocket* socket)
    : write_state_(WriteState::IDLE),
      socket_(socket),
      header_buffer_(
          new net::DrainableIOBuffer(new net::IOBuffer(kPacketHeaderSizeBytes),
                                     kPacketHeaderSizeBytes)),
      weak_factory_(this) {
  DCHECK(socket_);
}

StreamPacketWriter::~StreamPacketWriter() {}

void StreamPacketWriter::WritePacket(scoped_refptr<net::DrainableIOBuffer> data,
                                     const net::CompletionCallback& callback) {
  DCHECK_EQ(WriteState::IDLE, write_state_);
  DCHECK(data);
  CHECK(data->BytesRemaining());

  write_state_ = WriteState::HEADER;
  header_buffer_->SetOffset(0);
  *reinterpret_cast<uint32_t*>(header_buffer_->data()) =
      base::HostToNet32(data->BytesRemaining());
  payload_buffer_ = data;

  int result = DoWriteLoop(net::OK);
  if (result != net::ERR_IO_PENDING) {
    // Release the payload buffer, since the write operation has completed
    // synchronously.
    payload_buffer_ = nullptr;

    // Adapt synchronous completion to an asynchronous style.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, result));
  } else {
    callback_ = callback;
  }
}

int StreamPacketWriter::DoWriteLoop(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  DCHECK_GE(result, 0);
  DCHECK_NE(WriteState::IDLE, write_state_);

  while (result >= 0 && write_state_ != WriteState::IDLE) {
    VLOG(2) << "DoWriteLoop (state=" << write_state_ << ", result=" << result
            << ")";

    switch (write_state_) {
      case WriteState::HEADER:
        result = DoWriteHeader(result);
        break;
      case WriteState::PAYLOAD:
        result = DoWritePayload(result);
        break;
      case WriteState::IDLE:
        NOTREACHED();
        result = net::ERR_UNEXPECTED;
        break;
    }
  }

  return result;
}

int StreamPacketWriter::DoWriteHeader(int result) {
  DCHECK_EQ(WriteState::HEADER, write_state_);
  DCHECK_GE(result, 0);

  header_buffer_->DidConsume(result);
  if (header_buffer_->BytesRemaining() > 0) {
    return socket_->Write(header_buffer_.get(),
                          header_buffer_->BytesRemaining(),
                          base::Bind(&StreamPacketWriter::OnWriteComplete,
                                     weak_factory_.GetWeakPtr()));
  }

  write_state_ = WriteState::PAYLOAD;
  return net::OK;
}

int StreamPacketWriter::DoWritePayload(int result) {
  DCHECK_EQ(WriteState::PAYLOAD, write_state_);
  DCHECK_GE(result, 0);

  payload_buffer_->DidConsume(result);
  if (payload_buffer_->BytesRemaining() > 0) {
    return socket_->Write(payload_buffer_.get(),
                          payload_buffer_->BytesRemaining(),
                          base::Bind(&StreamPacketWriter::OnWriteComplete,
                                     weak_factory_.GetWeakPtr()));
  }

  write_state_ = WriteState::IDLE;
  return net::OK;
}

void StreamPacketWriter::OnWriteComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);

  // If the write was succesful, then process the result.
  if (result > 0) {
    result = DoWriteLoop(result);
  }

  // If the write finished, either successfully or by error, inform the
  // caller.
  if (result != net::ERR_IO_PENDING) {
    payload_buffer_ = nullptr;
    base::ResetAndReturn(&callback_).Run(result);
  }
}

}  // namespace blimp

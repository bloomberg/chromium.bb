// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_transport.h"

#include <string>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/cast_channel/cast_framer.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/browser/api/cast_channel/logger_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

#define VLOG_WITH_CONNECTION(level)                                           \
  VLOG(level) << "[" << ip_endpoint_.ToString() << ", auth=" << channel_auth_ \
              << "] "

namespace extensions {
namespace core_api {
namespace cast_channel {

CastTransportImpl::CastTransportImpl(net::Socket* socket,
                                     int channel_id,
                                     const net::IPEndPoint& ip_endpoint,
                                     ChannelAuthType channel_auth,
                                     scoped_refptr<Logger> logger)
    : started_(false),
      socket_(socket),
      write_state_(WRITE_STATE_NONE),
      read_state_(READ_STATE_NONE),
      error_state_(CHANNEL_ERROR_NONE),
      channel_id_(channel_id),
      ip_endpoint_(ip_endpoint),
      channel_auth_(channel_auth),
      logger_(logger) {
  DCHECK(socket);

  // Buffer is reused across messages to minimize unnecessary buffer
  // [re]allocations.
  read_buffer_ = new net::GrowableIOBuffer();
  read_buffer_->SetCapacity(MessageFramer::MessageHeader::max_message_size());
  framer_.reset(new MessageFramer(read_buffer_));
}

CastTransportImpl::~CastTransportImpl() {
  DCHECK(CalledOnValidThread());
  FlushWriteQueue();
}

// static
proto::ReadState CastTransportImpl::ReadStateToProto(
    CastTransportImpl::ReadState state) {
  switch (state) {
    case CastTransportImpl::READ_STATE_NONE:
      return proto::READ_STATE_NONE;
    case CastTransportImpl::READ_STATE_READ:
      return proto::READ_STATE_READ;
    case CastTransportImpl::READ_STATE_READ_COMPLETE:
      return proto::READ_STATE_READ_COMPLETE;
    case CastTransportImpl::READ_STATE_DO_CALLBACK:
      return proto::READ_STATE_DO_CALLBACK;
    case CastTransportImpl::READ_STATE_ERROR:
      return proto::READ_STATE_ERROR;
    default:
      NOTREACHED();
      return proto::READ_STATE_NONE;
  }
}

// static
proto::WriteState CastTransportImpl::WriteStateToProto(
    CastTransportImpl::WriteState state) {
  switch (state) {
    case CastTransportImpl::WRITE_STATE_NONE:
      return proto::WRITE_STATE_NONE;
    case CastTransportImpl::WRITE_STATE_WRITE:
      return proto::WRITE_STATE_WRITE;
    case CastTransportImpl::WRITE_STATE_WRITE_COMPLETE:
      return proto::WRITE_STATE_WRITE_COMPLETE;
    case CastTransportImpl::WRITE_STATE_DO_CALLBACK:
      return proto::WRITE_STATE_DO_CALLBACK;
    case CastTransportImpl::WRITE_STATE_ERROR:
      return proto::WRITE_STATE_ERROR;
    default:
      NOTREACHED();
      return proto::WRITE_STATE_NONE;
  }
}

// static
proto::ErrorState CastTransportImpl::ErrorStateToProto(ChannelError state) {
  switch (state) {
    case CHANNEL_ERROR_NONE:
      return proto::CHANNEL_ERROR_NONE;
    case CHANNEL_ERROR_CHANNEL_NOT_OPEN:
      return proto::CHANNEL_ERROR_CHANNEL_NOT_OPEN;
    case CHANNEL_ERROR_AUTHENTICATION_ERROR:
      return proto::CHANNEL_ERROR_AUTHENTICATION_ERROR;
    case CHANNEL_ERROR_CONNECT_ERROR:
      return proto::CHANNEL_ERROR_CONNECT_ERROR;
    case CHANNEL_ERROR_SOCKET_ERROR:
      return proto::CHANNEL_ERROR_SOCKET_ERROR;
    case CHANNEL_ERROR_TRANSPORT_ERROR:
      return proto::CHANNEL_ERROR_TRANSPORT_ERROR;
    case CHANNEL_ERROR_INVALID_MESSAGE:
      return proto::CHANNEL_ERROR_INVALID_MESSAGE;
    case CHANNEL_ERROR_INVALID_CHANNEL_ID:
      return proto::CHANNEL_ERROR_INVALID_CHANNEL_ID;
    case CHANNEL_ERROR_CONNECT_TIMEOUT:
      return proto::CHANNEL_ERROR_CONNECT_TIMEOUT;
    case CHANNEL_ERROR_UNKNOWN:
      return proto::CHANNEL_ERROR_UNKNOWN;
    default:
      NOTREACHED();
      return proto::CHANNEL_ERROR_NONE;
  }
}

void CastTransportImpl::SetReadDelegate(scoped_ptr<Delegate> read_delegate) {
  DCHECK(CalledOnValidThread());
  DCHECK(read_delegate);
  read_delegate_ = read_delegate.Pass();
  if (started_) {
    read_delegate_->Start();
  }
}

void CastTransportImpl::FlushWriteQueue() {
  for (; !write_queue_.empty(); write_queue_.pop()) {
    net::CompletionCallback& callback = write_queue_.front().callback;
    callback.Run(net::ERR_FAILED);
    callback.Reset();
  }
}

void CastTransportImpl::SendMessage(const CastMessage& message,
                                    const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::string serialized_message;
  if (!MessageFramer::Serialize(message, &serialized_message)) {
    logger_->LogSocketEventForMessage(channel_id_, proto::SEND_MESSAGE_FAILED,
                                      message.namespace_(),
                                      "Error when serializing message.");
    callback.Run(net::ERR_FAILED);
    return;
  }
  WriteRequest write_request(
      message.namespace_(), serialized_message, callback);

  write_queue_.push(write_request);
  logger_->LogSocketEventForMessage(
      channel_id_, proto::MESSAGE_ENQUEUED, message.namespace_(),
      base::StringPrintf("Queue size: %" PRIuS, write_queue_.size()));
  if (write_state_ == WRITE_STATE_NONE) {
    SetWriteState(WRITE_STATE_WRITE);
    OnWriteResult(net::OK);
  }
}

CastTransportImpl::WriteRequest::WriteRequest(
    const std::string& namespace_,
    const std::string& payload,
    const net::CompletionCallback& callback)
    : message_namespace(namespace_), callback(callback) {
  VLOG(2) << "WriteRequest size: " << payload.size();
  io_buffer = new net::DrainableIOBuffer(new net::StringIOBuffer(payload),
                                         payload.size());
}

CastTransportImpl::WriteRequest::~WriteRequest() {
}

void CastTransportImpl::SetReadState(ReadState read_state) {
  if (read_state_ != read_state) {
    read_state_ = read_state;
    logger_->LogSocketReadState(channel_id_, ReadStateToProto(read_state_));
  }
}

void CastTransportImpl::SetWriteState(WriteState write_state) {
  if (write_state_ != write_state) {
    write_state_ = write_state;
    logger_->LogSocketWriteState(channel_id_, WriteStateToProto(write_state_));
  }
}

void CastTransportImpl::SetErrorState(ChannelError error_state) {
  VLOG_WITH_CONNECTION(2) << "SetErrorState: " << error_state;
  error_state_ = error_state;
}

void CastTransportImpl::OnWriteResult(int result) {
  DCHECK(CalledOnValidThread());
  if (write_queue_.empty()) {
    SetWriteState(WRITE_STATE_NONE);
    return;
  }

  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    VLOG_WITH_CONNECTION(2) << "OnWriteResult (state=" << write_state_ << ", "
                            << "result=" << rv << ", "
                            << "queue size=" << write_queue_.size() << ")";

    WriteState state = write_state_;
    write_state_ = WRITE_STATE_NONE;
    switch (state) {
      case WRITE_STATE_WRITE:
        rv = DoWrite();
        break;
      case WRITE_STATE_WRITE_COMPLETE:
        rv = DoWriteComplete(rv);
        break;
      case WRITE_STATE_DO_CALLBACK:
        rv = DoWriteCallback();
        break;
      case WRITE_STATE_ERROR:
        rv = DoWriteError(rv);
        break;
      default:
        NOTREACHED() << "BUG in write flow. Unknown state: " << state;
        break;
    }
  } while (!write_queue_.empty() && rv != net::ERR_IO_PENDING &&
           write_state_ != WRITE_STATE_NONE);

  // No state change occurred in the switch case above. This means the
  // write operation has completed.
  if (write_state_ == WRITE_STATE_NONE) {
    logger_->LogSocketWriteState(channel_id_, WriteStateToProto(write_state_));
  }

  // If write loop is done because the queue is empty then set write
  // state to NONE
  if (write_queue_.empty()) {
    SetWriteState(WRITE_STATE_NONE);
  }

  // Write loop is done - flush the remaining write operations if an error
  // was encountered.
  if (rv == net::ERR_FAILED) {
    DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
    FlushWriteQueue();
  }
}

int CastTransportImpl::DoWrite() {
  DCHECK(!write_queue_.empty());
  WriteRequest& request = write_queue_.front();

  VLOG_WITH_CONNECTION(2) << "WriteData byte_count = "
                          << request.io_buffer->size() << " bytes_written "
                          << request.io_buffer->BytesConsumed();

  SetWriteState(WRITE_STATE_WRITE_COMPLETE);

  int rv = socket_->Write(
      request.io_buffer.get(), request.io_buffer->BytesRemaining(),
      base::Bind(&CastTransportImpl::OnWriteResult, base::Unretained(this)));
  logger_->LogSocketEventWithRv(channel_id_, proto::SOCKET_WRITE, rv);
  return rv;
}

int CastTransportImpl::DoWriteComplete(int result) {
  VLOG_WITH_CONNECTION(2) << "DoWriteComplete result=" << result;
  DCHECK(!write_queue_.empty());
  if (result <= 0) {  // NOTE that 0 also indicates an error
    SetErrorState(CHANNEL_ERROR_SOCKET_ERROR);
    SetWriteState(WRITE_STATE_ERROR);
    return result == 0 ? net::ERR_FAILED : result;
  }

  // Some bytes were successfully written
  WriteRequest& request = write_queue_.front();
  scoped_refptr<net::DrainableIOBuffer> io_buffer = request.io_buffer;
  io_buffer->DidConsume(result);
  if (io_buffer->BytesRemaining() == 0) {  // Message fully sent
    SetWriteState(WRITE_STATE_DO_CALLBACK);
  } else {
    SetWriteState(WRITE_STATE_WRITE);
  }

  return net::OK;
}

int CastTransportImpl::DoWriteCallback() {
  VLOG_WITH_CONNECTION(2) << "DoWriteCallback";
  DCHECK(!write_queue_.empty());

  SetWriteState(WRITE_STATE_WRITE);

  WriteRequest& request = write_queue_.front();
  int bytes_consumed = request.io_buffer->BytesConsumed();
  logger_->LogSocketEventForMessage(
      channel_id_, proto::MESSAGE_WRITTEN, request.message_namespace,
      base::StringPrintf("Bytes: %d", bytes_consumed));
  request.callback.Run(net::OK);
  write_queue_.pop();
  return net::OK;
}

int CastTransportImpl::DoWriteError(int result) {
  VLOG_WITH_CONNECTION(2) << "DoWriteError result=" << result;
  DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
  DCHECK_LT(result, 0);
  return net::ERR_FAILED;
}

void CastTransportImpl::Start() {
  DCHECK(CalledOnValidThread());
  DCHECK(!started_);
  DCHECK(read_delegate_)
      << "Read delegate must be set prior to calling Start()";
  read_delegate_->Start();
  if (read_state_ == READ_STATE_NONE) {
    // Initialize and run the read state machine.
    SetReadState(READ_STATE_READ);
    OnReadResult(net::OK);
  }
  started_ = true;
}

void CastTransportImpl::OnReadResult(int result) {
  DCHECK(CalledOnValidThread());
  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    VLOG_WITH_CONNECTION(2) << "OnReadResult(state=" << read_state_
                            << ", result=" << rv << ")";
    ReadState state = read_state_;
    read_state_ = READ_STATE_NONE;

    switch (state) {
      case READ_STATE_READ:
        rv = DoRead();
        break;
      case READ_STATE_READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case READ_STATE_DO_CALLBACK:
        rv = DoReadCallback();
        break;
      case READ_STATE_ERROR:
        rv = DoReadError(rv);
        DCHECK_EQ(read_state_, READ_STATE_NONE);
        break;
      default:
        NOTREACHED() << "BUG in read flow. Unknown state: " << state;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && read_state_ != READ_STATE_NONE);

  // No state change occurred in do-while loop above. This means state has
  // transitioned to NONE.
  if (read_state_ == READ_STATE_NONE) {
    logger_->LogSocketReadState(channel_id_, ReadStateToProto(read_state_));
  }

  if (rv == net::ERR_FAILED) {
    VLOG_WITH_CONNECTION(2) << "Sending OnError().";
    DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
    read_delegate_->OnError(error_state_, logger_->GetLastErrors(channel_id_));
  }
}

int CastTransportImpl::DoRead() {
  VLOG_WITH_CONNECTION(2) << "DoRead";
  SetReadState(READ_STATE_READ_COMPLETE);

  // Determine how many bytes need to be read.
  size_t num_bytes_to_read = framer_->BytesRequested();
  DCHECK_GT(num_bytes_to_read, 0u);

  // Read up to num_bytes_to_read into |current_read_buffer_|.
  return socket_->Read(
      read_buffer_.get(), base::checked_cast<uint32>(num_bytes_to_read),
      base::Bind(&CastTransportImpl::OnReadResult, base::Unretained(this)));
}

int CastTransportImpl::DoReadComplete(int result) {
  VLOG_WITH_CONNECTION(2) << "DoReadComplete result = " << result;

  if (result <= 0) {
    VLOG_WITH_CONNECTION(1) << "Read error, peer closed the socket.";
    SetErrorState(CHANNEL_ERROR_SOCKET_ERROR);
    SetReadState(READ_STATE_ERROR);
    return result == 0 ? net::ERR_FAILED : result;
  }

  size_t message_size;
  DCHECK(!current_message_);
  ChannelError framing_error;
  current_message_ = framer_->Ingest(result, &message_size, &framing_error);
  if (current_message_.get() && (framing_error == CHANNEL_ERROR_NONE)) {
    DCHECK_GT(message_size, static_cast<size_t>(0));
    logger_->LogSocketEventForMessage(
        channel_id_, proto::MESSAGE_READ, current_message_->namespace_(),
        base::StringPrintf("Message size: %u",
                           static_cast<uint32>(message_size)));
    SetReadState(READ_STATE_DO_CALLBACK);
  } else if (framing_error != CHANNEL_ERROR_NONE) {
    DCHECK(!current_message_);
    SetErrorState(CHANNEL_ERROR_INVALID_MESSAGE);
    SetReadState(READ_STATE_ERROR);
  } else {
    DCHECK(!current_message_);
    SetReadState(READ_STATE_READ);
  }
  return net::OK;
}

int CastTransportImpl::DoReadCallback() {
  VLOG_WITH_CONNECTION(2) << "DoReadCallback";
  if (!IsCastMessageValid(*current_message_)) {
    SetReadState(READ_STATE_ERROR);
    SetErrorState(CHANNEL_ERROR_INVALID_MESSAGE);
    return net::ERR_INVALID_RESPONSE;
  }
  SetReadState(READ_STATE_READ);
  logger_->LogSocketEventForMessage(channel_id_, proto::NOTIFY_ON_MESSAGE,
                                    current_message_->namespace_(),
                                    std::string());

  read_delegate_->OnMessage(*current_message_);
  current_message_.reset();
  return net::OK;
}

int CastTransportImpl::DoReadError(int result) {
  VLOG_WITH_CONNECTION(2) << "DoReadError";
  DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
  DCHECK_LE(result, 0);
  return net::ERR_FAILED;
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

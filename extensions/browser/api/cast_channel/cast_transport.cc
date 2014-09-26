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

#define VLOG_WITH_CONNECTION(level)                       \
  VLOG(level) << "[" << socket_->ip_endpoint().ToString() \
              << ", auth=" << socket_->channel_auth() << "] "

namespace extensions {
namespace core_api {
namespace cast_channel {

CastTransport::CastTransport(CastSocketInterface* socket,
                             Delegate* read_delegate,
                             scoped_refptr<Logger> logger)
    : socket_(socket),
      read_delegate_(read_delegate),
      write_state_(WRITE_STATE_NONE),
      read_state_(READ_STATE_NONE),
      logger_(logger) {
  DCHECK(socket);
  DCHECK(read_delegate);

  // Buffer is reused across messages to minimize unnecessary buffer
  // [re]allocations.
  read_buffer_ = new net::GrowableIOBuffer();
  read_buffer_->SetCapacity(MessageFramer::MessageHeader::max_message_size());
  framer_.reset(new MessageFramer(read_buffer_));
}

CastTransport::~CastTransport() {
  DCHECK(thread_checker_.CalledOnValidThread());
  FlushWriteQueue();
}

// static
proto::ReadState CastTransport::ReadStateToProto(
    CastTransport::ReadState state) {
  switch (state) {
    case CastTransport::READ_STATE_NONE:
      return proto::READ_STATE_NONE;
    case CastTransport::READ_STATE_READ:
      return proto::READ_STATE_READ;
    case CastTransport::READ_STATE_READ_COMPLETE:
      return proto::READ_STATE_READ_COMPLETE;
    case CastTransport::READ_STATE_DO_CALLBACK:
      return proto::READ_STATE_DO_CALLBACK;
    case CastTransport::READ_STATE_ERROR:
      return proto::READ_STATE_ERROR;
    default:
      NOTREACHED();
      return proto::READ_STATE_NONE;
  }
}

// static
proto::WriteState CastTransport::WriteStateToProto(
    CastTransport::WriteState state) {
  switch (state) {
    case CastTransport::WRITE_STATE_NONE:
      return proto::WRITE_STATE_NONE;
    case CastTransport::WRITE_STATE_WRITE:
      return proto::WRITE_STATE_WRITE;
    case CastTransport::WRITE_STATE_WRITE_COMPLETE:
      return proto::WRITE_STATE_WRITE_COMPLETE;
    case CastTransport::WRITE_STATE_DO_CALLBACK:
      return proto::WRITE_STATE_DO_CALLBACK;
    case CastTransport::WRITE_STATE_ERROR:
      return proto::WRITE_STATE_ERROR;
    default:
      NOTREACHED();
      return proto::WRITE_STATE_NONE;
  }
}

// static
proto::ErrorState CastTransport::ErrorStateToProto(ChannelError state) {
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

void CastTransport::FlushWriteQueue() {
  for (; !write_queue_.empty(); write_queue_.pop()) {
    net::CompletionCallback& callback = write_queue_.front().callback;
    callback.Run(net::ERR_FAILED);
    callback.Reset();
  }
}

void CastTransport::SendMessage(const CastMessage& message,
                                const net::CompletionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string serialized_message;
  if (!MessageFramer::Serialize(message, &serialized_message)) {
    logger_->LogSocketEventForMessage(socket_->id(),
                                      proto::SEND_MESSAGE_FAILED,
                                      message.namespace_(),
                                      "Error when serializing message.");
    callback.Run(net::ERR_FAILED);
    return;
  }
  WriteRequest write_request(
      message.namespace_(), serialized_message, callback);

  write_queue_.push(write_request);
  logger_->LogSocketEventForMessage(
      socket_->id(),
      proto::MESSAGE_ENQUEUED,
      message.namespace_(),
      base::StringPrintf("Queue size: %" PRIuS, write_queue_.size()));
  if (write_state_ == WRITE_STATE_NONE) {
    SetWriteState(WRITE_STATE_WRITE);
    OnWriteResult(net::OK);
  }
}

CastTransport::WriteRequest::WriteRequest(
    const std::string& namespace_,
    const std::string& payload,
    const net::CompletionCallback& callback)
    : message_namespace(namespace_), callback(callback) {
  VLOG(2) << "WriteRequest size: " << payload.size();
  io_buffer = new net::DrainableIOBuffer(new net::StringIOBuffer(payload),
                                         payload.size());
}

CastTransport::WriteRequest::~WriteRequest() {
}

void CastTransport::SetReadState(ReadState read_state) {
  if (read_state_ != read_state) {
    read_state_ = read_state;
    logger_->LogSocketReadState(socket_->id(), ReadStateToProto(read_state_));
  }
}

void CastTransport::SetWriteState(WriteState write_state) {
  if (write_state_ != write_state) {
    write_state_ = write_state;
    logger_->LogSocketWriteState(socket_->id(),
                                 WriteStateToProto(write_state_));
  }
}

void CastTransport::SetErrorState(ChannelError error_state) {
  if (error_state_ != error_state) {
    error_state_ = error_state;
    logger_->LogSocketErrorState(socket_->id(),
                                 ErrorStateToProto(error_state_));
  }
}

void CastTransport::OnWriteResult(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG_WITH_CONNECTION(1) << "OnWriteResult queue size: "
                          << write_queue_.size();

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

  // No state change occurred in do-while loop above. This means state has
  // transitioned to NONE.
  if (write_state_ == WRITE_STATE_NONE) {
    logger_->LogSocketWriteState(socket_->id(),
                                 WriteStateToProto(write_state_));
  }

  // If write loop is done because the queue is empty then set write
  // state to NONE
  if (write_queue_.empty()) {
    SetWriteState(WRITE_STATE_NONE);
  }

  // Write loop is done - if the result is ERR_FAILED then close with error.
  if (rv == net::ERR_FAILED) {
    DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
    socket_->CloseWithError(error_state_);
    FlushWriteQueue();
  }
}

int CastTransport::DoWrite() {
  DCHECK(!write_queue_.empty());
  WriteRequest& request = write_queue_.front();

  VLOG_WITH_CONNECTION(2) << "WriteData byte_count = "
                          << request.io_buffer->size() << " bytes_written "
                          << request.io_buffer->BytesConsumed();

  SetWriteState(WRITE_STATE_WRITE_COMPLETE);

  int rv = socket_->Write(
      request.io_buffer.get(),
      request.io_buffer->BytesRemaining(),
      base::Bind(&CastTransport::OnWriteResult, base::Unretained(this)));
  logger_->LogSocketEventWithRv(socket_->id(), proto::SOCKET_WRITE, rv);

  return rv;
}

int CastTransport::DoWriteComplete(int result) {
  VLOG_WITH_CONNECTION(2) << "DoWriteComplete result=" << result;
  DCHECK(!write_queue_.empty());
  if (result <= 0) {  // NOTE that 0 also indicates an error
    SetErrorState(CHANNEL_ERROR_TRANSPORT_ERROR);
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

int CastTransport::DoWriteCallback() {
  VLOG_WITH_CONNECTION(2) << "DoWriteCallback";
  DCHECK(!write_queue_.empty());

  SetWriteState(WRITE_STATE_WRITE);

  WriteRequest& request = write_queue_.front();
  int bytes_consumed = request.io_buffer->BytesConsumed();
  logger_->LogSocketEventForMessage(
      socket_->id(),
      proto::MESSAGE_WRITTEN,
      request.message_namespace,
      base::StringPrintf("Bytes: %d", bytes_consumed));
  request.callback.Run(net::OK);
  write_queue_.pop();
  return net::OK;
}

int CastTransport::DoWriteError(int result) {
  VLOG_WITH_CONNECTION(2) << "DoWriteError result=" << result;
  DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
  DCHECK_LT(result, 0);
  return net::ERR_FAILED;
}

void CastTransport::StartReadLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Read loop would have already been started if read state is not NONE
  if (read_state_ == READ_STATE_NONE) {
    SetReadState(READ_STATE_READ);
    OnReadResult(net::OK);
  }
}

void CastTransport::OnReadResult(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
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
    logger_->LogSocketReadState(socket_->id(), ReadStateToProto(read_state_));
  }

  if (rv == net::ERR_FAILED) {
    DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
    socket_->CloseWithError(error_state_);
    FlushWriteQueue();
    read_delegate_->OnError(
        socket_, error_state_, logger_->GetLastErrors(socket_->id()));
  }
}

int CastTransport::DoRead() {
  VLOG_WITH_CONNECTION(2) << "DoRead";
  SetReadState(READ_STATE_READ_COMPLETE);

  // Determine how many bytes need to be read.
  size_t num_bytes_to_read = framer_->BytesRequested();

  // Read up to num_bytes_to_read into |current_read_buffer_|.
  int rv = socket_->Read(
      read_buffer_.get(),
      base::checked_cast<uint32>(num_bytes_to_read),
      base::Bind(&CastTransport::OnReadResult, base::Unretained(this)));

  return rv;
}

int CastTransport::DoReadComplete(int result) {
  VLOG_WITH_CONNECTION(2) << "DoReadComplete result = " << result;

  if (result <= 0) {
    SetErrorState(CHANNEL_ERROR_TRANSPORT_ERROR);
    SetReadState(READ_STATE_ERROR);
    return result == 0 ? net::ERR_FAILED : result;
  }

  size_t message_size;
  DCHECK(current_message_.get() == NULL);
  current_message_ = framer_->Ingest(result, &message_size, &error_state_);
  if (current_message_.get()) {
    DCHECK_EQ(error_state_, CHANNEL_ERROR_NONE);
    DCHECK_GT(message_size, static_cast<size_t>(0));
    logger_->LogSocketEventForMessage(
        socket_->id(),
        proto::MESSAGE_READ,
        current_message_->namespace_(),
        base::StringPrintf("Message size: %u",
                           static_cast<uint32>(message_size)));
    SetReadState(READ_STATE_DO_CALLBACK);
  } else if (error_state_ != CHANNEL_ERROR_NONE) {
    DCHECK(current_message_.get() == NULL);
    SetErrorState(CHANNEL_ERROR_INVALID_MESSAGE);
    SetReadState(READ_STATE_ERROR);
  } else {
    DCHECK(current_message_.get() == NULL);
    SetReadState(READ_STATE_READ);
  }
  return net::OK;
}

int CastTransport::DoReadCallback() {
  VLOG_WITH_CONNECTION(2) << "DoReadCallback";
  SetReadState(READ_STATE_READ);
  if (!IsCastMessageValid(*current_message_)) {
    SetReadState(READ_STATE_ERROR);
    SetErrorState(CHANNEL_ERROR_INVALID_MESSAGE);
    return net::ERR_INVALID_RESPONSE;
  }
  logger_->LogSocketEventForMessage(socket_->id(),
                                    proto::NOTIFY_ON_MESSAGE,
                                    current_message_->namespace_(),
                                    std::string());
  read_delegate_->OnMessage(socket_, *current_message_);
  current_message_.reset();
  return net::OK;
}

int CastTransport::DoReadError(int result) {
  VLOG_WITH_CONNECTION(2) << "DoReadError";
  DCHECK_NE(CHANNEL_ERROR_NONE, error_state_);
  DCHECK_LE(result, 0);
  return net::ERR_FAILED;
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

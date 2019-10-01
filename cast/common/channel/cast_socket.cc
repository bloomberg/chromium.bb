// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/cast_socket.h"

#include "cast/common/channel/message_framer.h"
#include "platform/api/logging.h"

namespace cast {
namespace channel {

using message_serialization::DeserializeResult;
using openscreen::ErrorOr;
using openscreen::platform::TlsConnection;

CastSocket::CastSocket(std::unique_ptr<TlsConnection> connection,
                       Client* client,
                       uint32_t socket_id)
    : client_(client),
      connection_(std::move(connection)),
      socket_id_(socket_id) {
  OSP_DCHECK(client);
  connection_->set_client(this);
}

CastSocket::~CastSocket() = default;

Error CastSocket::SendMessage(const CastMessage& message) {
  if (state_ == State::kError) {
    return Error::Code::kSocketClosedFailure;
  }

  const ErrorOr<std::vector<uint8_t>> out =
      message_serialization::Serialize(message);
  if (!out) {
    return out.error();
  }

  if (state_ == State::kBlocked) {
    message_queue_.emplace_back(std::move(out.value()));
    return Error::Code::kNone;
  }

  connection_->Write(out.value().data(), out.value().size());
  return Error::Code::kNone;
}

void CastSocket::OnWriteBlocked(TlsConnection* connection) {
  if (state_ == State::kOpen) {
    state_ = State::kBlocked;
  }
}

void CastSocket::OnWriteUnblocked(TlsConnection* connection) {
  if (state_ == State::kBlocked) {
    state_ = State::kOpen;
    for (const auto& message : message_queue_) {
      connection_->Write(message.data(), message.size());
    }
    OSP_DCHECK(state_ == State::kOpen) << static_cast<int>(state_);
    message_queue_.clear();
  }
}

void CastSocket::OnError(TlsConnection* connection, Error error) {
  state_ = State::kError;
  client_->OnError(this, error);
}

void CastSocket::OnRead(TlsConnection* connection, std::vector<uint8_t> block) {
  read_buffer_.insert(read_buffer_.end(), block.begin(), block.end());
  ErrorOr<DeserializeResult> message_or_error =
      message_serialization::TryDeserialize(
          absl::Span<uint8_t>(&read_buffer_[0], read_buffer_.size()));
  if (!message_or_error) {
    return;
  }
  read_buffer_.erase(read_buffer_.begin(),
                     read_buffer_.begin() + message_or_error.value().length);
  client_->OnMessage(this, std::move(message_or_error.value().message));
}

}  // namespace channel
}  // namespace cast

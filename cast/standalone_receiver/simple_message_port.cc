// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/simple_message_port.h"

#include <utility>

namespace openscreen {
namespace cast {

SimpleMessagePort::SimpleMessagePort() = default;
SimpleMessagePort::~SimpleMessagePort() = default;

void SimpleMessagePort::SetClient(MessagePort::Client* client) {
  client_ = client;
}

void SimpleMessagePort::ReceiveMessage(absl::string_view message) {
  client_->OnMessage("sender-id", "namespace", message);
}

void SimpleMessagePort::ReceiveError(Error error) {
  client_->OnError(error);
}

void SimpleMessagePort::PostMessage(absl::string_view sender_id,
                                    absl::string_view message_namespace,
                                    absl::string_view message) {
  posted_messages_.emplace_back(std::move(message));
}

}  // namespace cast
}  // namespace openscreen
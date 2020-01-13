// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_SIMPLE_MESSAGE_PORT_H_
#define CAST_STANDALONE_RECEIVER_SIMPLE_MESSAGE_PORT_H_

#include <string>
#include <vector>

#include "cast/streaming/receiver_session.h"

namespace openscreen {
namespace cast {

class SimpleMessagePort : public MessagePort {
 public:
  SimpleMessagePort();
  ~SimpleMessagePort() override;

  void SetClient(MessagePort::Client* client) override;

  void ReceiveMessage(absl::string_view message);
  void ReceiveError(Error error);
  void PostMessage(absl::string_view sender_id,
                   absl::string_view message_namespace,
                   absl::string_view message) override;

  MessagePort::Client* client() const { return client_; }
  const std::vector<std::string>& posted_messages() const {
    return posted_messages_;
  }

 private:
  MessagePort::Client* client_ = nullptr;
  std::vector<std::string> posted_messages_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_SIMPLE_MESSAGE_PORT_H_

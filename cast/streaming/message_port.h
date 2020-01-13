// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_PORT_H_
#define CAST_STREAMING_MESSAGE_PORT_H_

#include "absl/strings/string_view.h"
#include "platform/base/error.h"

namespace openscreen {
namespace cast {

// This interface is intended to provide an abstraction for communicating
// cast messages across a pipe with guaranteed delivery. This is used to
// decouple the cast receiver session (and potentially other classes) from any
// network implementation.
class MessagePort {
 public:
  class Client {
   public:
    virtual void OnMessage(absl::string_view sender_id,
                           absl::string_view message_namespace,
                           absl::string_view message) = 0;
    virtual void OnError(Error error) = 0;
  };

  virtual ~MessagePort() = default;
  virtual void SetClient(Client* client) = 0;
  virtual void PostMessage(absl::string_view sender_id,
                           absl::string_view message_namespace,
                           absl::string_view message) = 0;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_MESSAGE_PORT_H_

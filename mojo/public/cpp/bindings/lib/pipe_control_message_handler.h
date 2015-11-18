// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_PIPE_CONTROL_MESSAGE_HANDLER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_PIPE_CONTROL_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/interface_id.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

class PipeControlMessageHandlerDelegate;

// Handler for messages defined in pipe_control_messages.mojom.
class PipeControlMessageHandler : public MessageReceiver {
 public:
  explicit PipeControlMessageHandler(
      PipeControlMessageHandlerDelegate* delegate);
  ~PipeControlMessageHandler() override;

  // NOTE: |message| must have passed message header validation.
  static bool IsPipeControlMessage(const Message* message);

  // MessageReceiver implementation:

  // NOTE: |message| must:
  //   - have passed message header validation; and
  //   - be a pipe control message (i.e., IsPipeControlMessage() returns true).
  // If the method returns false, the message pipe should be closed.
  bool Accept(Message* message) override;

 private:
  // |message| must have passed message header validation.
  bool Validate(const Message* message);
  bool RunOrClosePipe(Message* message);

  PipeControlMessageHandlerDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(PipeControlMessageHandler);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_PIPE_CONTROL_MESSAGE_HANDLER_H_

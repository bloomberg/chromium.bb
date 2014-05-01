// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

class MessageHeaderValidator : public MessageReceiver {
 public:
  explicit MessageHeaderValidator(MessageReceiver* next);

  virtual bool Accept(Message* message) MOJO_OVERRIDE;
  virtual bool AcceptWithResponder(Message* message, MessageReceiver* responder)
      MOJO_OVERRIDE;

 private:
  MessageReceiver* next_;
};

}  // namespace internal
}  // namespace mojo

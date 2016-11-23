// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_FAKE_BLIMP_MESSAGE_PROCESSOR_H_
#define BLIMP_NET_FAKE_BLIMP_MESSAGE_PROCESSOR_H_

#include <vector>

#include "blimp/net/blimp_message_processor.h"

namespace blimp {

// Stores messages in a vector for later inspection.
class FakeBlimpMessageProcessor : public BlimpMessageProcessor {
 public:
  FakeBlimpMessageProcessor();
  ~FakeBlimpMessageProcessor() override;

  void ProcessMessage(
      std::unique_ptr<BlimpMessage> message,
      const net::CompletionCallback& callback) override;

  const std::vector<BlimpMessage>& messages() {
    return messages_;
  }

 private:
  std::vector<BlimpMessage> messages_;

  DISALLOW_COPY_AND_ASSIGN(FakeBlimpMessageProcessor);
};

}  // namespace blimp

#endif  // BLIMP_NET_FAKE_BLIMP_MESSAGE_PROCESSOR_H_

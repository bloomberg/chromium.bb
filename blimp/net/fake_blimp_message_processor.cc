// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/fake_blimp_message_processor.h"

#include "blimp/common/proto/blimp_message.pb.h"

namespace blimp {

FakeBlimpMessageProcessor::FakeBlimpMessageProcessor() = default;

FakeBlimpMessageProcessor::~FakeBlimpMessageProcessor() = default;

void FakeBlimpMessageProcessor::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  messages_.push_back(*message);
}

}  // namespace blimp

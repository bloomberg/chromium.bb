// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/null_blimp_message_processor.h"

namespace blimp {

NullBlimpMessageProcessor::~NullBlimpMessageProcessor() {}

void NullBlimpMessageProcessor::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  if (!callback.is_null())
    callback.Run(net::OK);
}

}  // namespace blimp

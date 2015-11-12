// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_output_buffer.h"

#include "base/macros.h"

namespace blimp {

BlimpMessageOutputBuffer::BlimpMessageOutputBuffer() {
  NOTIMPLEMENTED();
}

BlimpMessageOutputBuffer::~BlimpMessageOutputBuffer() {
  NOTIMPLEMENTED();
}

void BlimpMessageOutputBuffer::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  NOTIMPLEMENTED();
}

void BlimpMessageOutputBuffer::OnMessageCheckpoint(int64 message_id) {
  NOTIMPLEMENTED();
}

}  // namespace blimp

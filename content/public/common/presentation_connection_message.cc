// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/presentation_connection_message.h"

namespace content {

PresentationConnectionMessage::PresentationConnectionMessage(
    PresentationMessageType type)
    : type(type) {}

PresentationConnectionMessage::~PresentationConnectionMessage() {}

bool PresentationConnectionMessage::is_binary() const {
  return data != nullptr;
}

}  // namespace content

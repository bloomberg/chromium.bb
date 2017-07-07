// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/presentation_connection_message.h"

#include <utility>

namespace content {

// TODO(crbug.com/524128): This restriction comes from Cast.  Raise this limit
// for non-Cast presentations.
const size_t kMaxPresentationConnectionMessageSize = 64 * 1024;  // 64 KB.

PresentationConnectionMessage::PresentationConnectionMessage() = default;

PresentationConnectionMessage::PresentationConnectionMessage(
    std::string message)
    : message(std::move(message)) {}

PresentationConnectionMessage::PresentationConnectionMessage(
    std::vector<uint8_t> data)
    : data(std::move(data)) {}

PresentationConnectionMessage::PresentationConnectionMessage(
    const PresentationConnectionMessage& other) = default;

// Note: "move constructor noexcept = default" currently does not compile on
// Windows and Android (crbug.com/706963).
PresentationConnectionMessage::PresentationConnectionMessage(
    PresentationConnectionMessage&& other) noexcept
    : message(std::move(other.message)), data(std::move(other.data)) {}

PresentationConnectionMessage::~PresentationConnectionMessage() {}

bool PresentationConnectionMessage::operator==(
    const PresentationConnectionMessage& other) const {
  return this->data == other.data && this->message == other.message;
}

PresentationConnectionMessage& PresentationConnectionMessage::operator=(
    const PresentationConnectionMessage& other) = default;

PresentationConnectionMessage& PresentationConnectionMessage::operator=(
    PresentationConnectionMessage&& other) = default;

bool PresentationConnectionMessage::is_binary() const {
  return data.has_value();
}

}  // namespace content

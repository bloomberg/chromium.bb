// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/fake_secure_context.h"

namespace proximity_auth {

namespace {
const char kFakeEncodingSuffix[] = ", but encoded";
const char kChannelBindingData[] = "channel binding data";
}  // namespace

FakeSecureContext::FakeSecureContext()
    : protocol_version_(SecureContext::PROTOCOL_VERSION_THREE_ONE) {}

FakeSecureContext::~FakeSecureContext() {}

std::string FakeSecureContext::GetChannelBindingData() const {
  return kChannelBindingData;
}

SecureContext::ProtocolVersion FakeSecureContext::GetProtocolVersion() const {
  return protocol_version_;
}

void FakeSecureContext::Encode(const std::string& message,
                               const MessageCallback& callback) {
  callback.Run(message + kFakeEncodingSuffix);
}

void FakeSecureContext::Decode(const std::string& encoded_message,
                               const MessageCallback& callback) {
  size_t suffix_size = std::string(kFakeEncodingSuffix).size();
  if (encoded_message.size() < suffix_size &&
      encoded_message.substr(encoded_message.size() - suffix_size) !=
          kFakeEncodingSuffix) {
    callback.Run(std::string());
  }

  std::string decoded_message = encoded_message;
  decoded_message.erase(decoded_message.rfind(kFakeEncodingSuffix));
  callback.Run(decoded_message);
}

}  // namespace proximity_auth

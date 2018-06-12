// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"

#include "base/memory/ptr_util.h"

namespace chromeos {

namespace secure_channel {

FakeClientChannel::FakeClientChannel() = default;

FakeClientChannel::~FakeClientChannel() = default;

void FakeClientChannel::PerformGetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadata)> callback) {
  std::move(callback).Run(connection_metadata_);
}

void FakeClientChannel::PerformSendMessage(const std::string& payload,
                                           base::OnceClosure on_sent_callback) {
  sent_messages_.push_back(
      std::make_pair(payload, std::move(on_sent_callback)));
}

void FakeClientChannel::PerformDisconnection() {
  NotifyDisconnected();
}

}  // namespace secure_channel

}  // namespace chromeos

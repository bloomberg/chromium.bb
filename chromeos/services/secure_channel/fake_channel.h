// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CHANNEL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

namespace secure_channel {

// Test mojom::Channel implementation.
class FakeChannel : public mojom::Channel {
 public:
  FakeChannel();
  ~FakeChannel() override;

  mojom::ChannelPtr GenerateInterfacePtr();
  void DisconnectGeneratedPtrs();

  void set_connection_metadata(
      const mojom::ConnectionMetadata& connection_metadata) {
    connection_metadata_ = connection_metadata;
  }

  std::vector<std::pair<std::string, SendMessageCallback>>& sent_messages() {
    return sent_messages_;
  }

 private:
  // mojom::Channel:
  void SendMessage(const std::string& message,
                   SendMessageCallback callback) override;
  void GetConnectionMetadata(GetConnectionMetadataCallback callback) override;

  mojo::BindingSet<mojom::Channel> bindings_;

  std::vector<std::pair<std::string, SendMessageCallback>> sent_messages_;
  mojom::ConnectionMetadata connection_metadata_;

  DISALLOW_COPY_AND_ASSIGN(FakeChannel);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CHANNEL_H_

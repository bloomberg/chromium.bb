// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_H_

#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Test double implementation of ClientChannel.
class FakeClientChannel : public ClientChannel {
 public:
  FakeClientChannel();
  ~FakeClientChannel() override;

  using ClientChannel::NotifyDisconnected;
  using ClientChannel::NotifyMessageReceived;

  void InvokePendingGetConnectionMetadataCallback(
      mojom::ConnectionMetadataPtr connection_metadata);

  std::vector<std::pair<std::string, base::OnceClosure>>& sent_messages() {
    return sent_messages_;
  }

  void set_destructor_callback(base::OnceClosure callback) {
    destructor_callback_ = std::move(callback);
  }

 private:
  friend class SecureChannelClientChannelImplTest;

  // ClientChannel:
  void PerformGetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void PerformSendMessage(const std::string& payload,
                          base::OnceClosure on_sent_callback) override;

  // Queues up callbacks passed into PerformGetConnectionMetadata(), to be
  // invoked later.
  std::queue<base::OnceCallback<void(mojom::ConnectionMetadataPtr)>>
      get_connection_metadata_callback_queue_;
  std::vector<std::pair<std::string, base::OnceClosure>> sent_messages_;
  base::OnceClosure destructor_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeClientChannel);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_H_

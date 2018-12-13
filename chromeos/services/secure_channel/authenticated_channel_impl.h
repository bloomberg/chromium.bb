// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/authenticated_channel.h"
#include "chromeos/services/secure_channel/secure_channel.h"

namespace chromeos {

namespace secure_channel {

class SecureChannel;

// Concrete AuthenticatedChannel implementation, whose send/receive mechanisms
// are implemented via SecureChannel.
class AuthenticatedChannelImpl : public AuthenticatedChannel,
                                 public SecureChannel::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual std::unique_ptr<AuthenticatedChannel> BuildInstance(
        const std::vector<mojom::ConnectionCreationDetail>&
            connection_creation_details,
        std::unique_ptr<SecureChannel> secure_channel);

   private:
    static Factory* test_factory_;
  };

  ~AuthenticatedChannelImpl() override;

 private:
  AuthenticatedChannelImpl(const std::vector<mojom::ConnectionCreationDetail>&
                               connection_creation_details,
                           std::unique_ptr<SecureChannel> secure_channel);

  // AuthenticatedChannel:
  void GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void PerformSendMessage(const std::string& feature,
                          const std::string& payload,
                          base::OnceClosure on_sent_callback) final;
  void PerformDisconnection() override;

  // SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      SecureChannel* secure_channel,
      const SecureChannel::Status& old_status,
      const SecureChannel::Status& new_status) override;
  void OnMessageReceived(SecureChannel* secure_channel,
                         const std::string& feature,
                         const std::string& payload) override;
  void OnMessageSent(SecureChannel* secure_channel,
                     int sequence_number) override;

  void OnRssiFetched(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback,
      base::Optional<int32_t> current_rssi);

  const std::vector<mojom::ConnectionCreationDetail>
      connection_creation_details_;
  std::unique_ptr<SecureChannel> secure_channel_;
  std::unordered_map<int, base::OnceClosure> sequence_number_to_callback_map_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatedChannelImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_IMPL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_H_

#include <string>

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Proxies messages between clients and remote devices. When messages are sent
// from the client, this proxy notifies its delegate, and when messages are
// received from the remote device, HandleReceivedMessage() should be called so
// that the message can be passed to the client.
class SingleClientMessageProxy {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnSendMessageRequested(const std::string& message_feaure,
                                        const std::string& message_payload,
                                        base::OnceClosure on_sent_callback) = 0;
    virtual void GetConnectionMetadata(
        base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) = 0;
    virtual void OnClientDisconnected(
        const base::UnguessableToken& proxy_id) = 0;
  };

  virtual ~SingleClientMessageProxy();

  // Should be called when any message is received over the connection.
  virtual void HandleReceivedMessage(const std::string& feature,
                                     const std::string& payload) = 0;

  // Should be called when the underlying connection to the remote device has
  // been disconnected (e.g., because the other device closed the connection or
  // because of instability on the communication channel).
  virtual void HandleRemoteDeviceDisconnection() = 0;

  virtual const base::UnguessableToken& GetProxyId() = 0;

 protected:
  SingleClientMessageProxy(Delegate* delegate);

  void NotifySendMessageRequested(const std::string& message_feature,
                                  const std::string& message_payload,
                                  base::OnceClosure on_sent_callback);
  void NotifyClientDisconnected();
  void GetConnectionMetadataFromDelegate(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback);

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientMessageProxy);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// A full-duplex communication channel which is guaranteed to be authenticated
// (i.e., the two sides of the channel both belong to the same underlying user).
// All messages sent and received over the channel are encrypted.
//
// If clients wish to disconnect the channel, they simply need to delete the
// object.
class ClientChannel {
 public:
  class Observer {
   public:
    virtual ~Observer();
    virtual void OnDisconnected() = 0;
    virtual void OnMessageReceived(const std::string& payload) = 0;
  };

  virtual ~ClientChannel();

  bool GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback);

  // Sends a message with the specified |payload|. Once the message has been
  // sent, |on_sent_callback| will be invoked. Returns whether this
  // ClientChannel was able to start sending the message; this function only
  // fails if the underlying connection has been disconnected.
  bool SendMessage(const std::string& payload,
                   base::OnceClosure on_sent_callback);

  bool is_disconnected() const { return is_disconnected_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ClientChannel();

  // Performs the actual logic of sending the message. By the time this function
  // is called, it has already been confirmed that the channel has not been
  // disconnected.
  virtual void PerformSendMessage(const std::string& payload,
                                  base::OnceClosure on_sent_callback) = 0;

  virtual void PerformGetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) = 0;

  void NotifyDisconnected();
  void NotifyMessageReceived(const std::string& payload);

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
  bool is_disconnected_ = false;

  DISALLOW_COPY_AND_ASSIGN(ClientChannel);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_H_

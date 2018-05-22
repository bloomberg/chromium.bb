// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/authenticated_channel.h"

#include "base/callback.h"
#include "base/guid.h"

namespace chromeos {

namespace secure_channel {

AuthenticatedChannel::Observer::~Observer() = default;

AuthenticatedChannel::AuthenticatedChannel() = default;

AuthenticatedChannel::~AuthenticatedChannel() = default;

bool AuthenticatedChannel::SendMessage(const std::string& feature,
                                       const std::string& payload,
                                       base::OnceClosure on_sent_callback) {
  if (is_disconnected_)
    return false;

  PerformSendMessage(feature, payload, std::move(on_sent_callback));
  return true;
}

void AuthenticatedChannel::Disconnect() {
  // Clients should not attempt to disconnect an already-disconnected channel.
  DCHECK(!is_disconnected_);
  PerformDisconnection();
}

void AuthenticatedChannel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AuthenticatedChannel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AuthenticatedChannel::NotifyDisconnected() {
  is_disconnected_ = true;

  for (auto& observer : observer_list_)
    observer.OnDisconnected();
}

void AuthenticatedChannel::NotifyMessageReceived(const std::string& feature,
                                                 const std::string& payload) {
  // Make a copy before notifying observers to ensure that if one observer
  // deletes |this| before the next observer is able to be processed, a segfault
  // is prevented.
  const std::string feature_copy = feature;
  const std::string payload_copy = payload;

  for (auto& observer : observer_list_)
    observer.OnMessageReceived(feature_copy, payload_copy);
}

}  // namespace secure_channel

}  // namespace chromeos

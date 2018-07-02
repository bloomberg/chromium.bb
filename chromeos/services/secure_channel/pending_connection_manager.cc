// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_connection_manager.h"

#include "chromeos/services/secure_channel/authenticated_channel.h"

namespace chromeos {

namespace secure_channel {

PendingConnectionManager::PendingConnectionManager(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

PendingConnectionManager::~PendingConnectionManager() = default;

void PendingConnectionManager::NotifyOnConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
    const ConnectionDetails& connection_details) {
  delegate_->OnConnection(std::move(authenticated_channel), std::move(clients),
                          connection_details);
}

}  // namespace secure_channel

}  // namespace chromeos

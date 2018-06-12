// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/connection_medium.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/pending_connection_manager.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

// Concrete PendingConnectionManager implementation.
// TODO(khorimoto): Implement.
class PendingConnectionManagerImpl : public PendingConnectionManager {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<PendingConnectionManager> BuildInstance(
        Delegate* delegate);

   private:
    static Factory* test_factory_;
  };

  ~PendingConnectionManagerImpl() override;

 private:
  PendingConnectionManagerImpl(Delegate* delegate);

  void HandleConnectionRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority) override;

  DISALLOW_COPY_AND_ASSIGN(PendingConnectionManagerImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_

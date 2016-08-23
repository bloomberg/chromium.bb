// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_COORDINATOR_COMMON_CLIENT_REGISTRY_H_
#define COMPONENTS_MEMORY_COORDINATOR_COMMON_CLIENT_REGISTRY_H_

#include "base/observer_list_threadsafe.h"
#include "components/memory_coordinator/common/memory_coordinator_client.h"
#include "components/memory_coordinator/common/memory_coordinator_export.h"

namespace memory_coordinator {

// ClientRegistry is a base class of process-specific memory coordinator
// and provides ways to register/unregister MemoryCoordinatorClients.
class MEMORY_COORDINATOR_EXPORT ClientRegistry {
 public:
  ClientRegistry();
  virtual ~ClientRegistry();

  // Registers/unregisters a client. Does not take ownership of client.
  void RegisterClient(MemoryCoordinatorClient* client);
  void UnregisterClient(MemoryCoordinatorClient* client);

 protected:
  using ClientList = base::ObserverListThreadSafe<MemoryCoordinatorClient>;
  ClientList* clients() { return clients_.get(); }

 private:
  scoped_refptr<ClientList> clients_;
};

}  // namespace memory_coordinator

#endif  // COMPONENTS_MEMORY_COORDINATOR_COMMON_CLIENT_REGISTRY_H_

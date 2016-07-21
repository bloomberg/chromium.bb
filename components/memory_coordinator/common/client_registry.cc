// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/common/client_registry.h"

namespace memory_coordinator {

ClientRegistry::ClientRegistry() : clients_(new ClientList) {}

ClientRegistry::~ClientRegistry() {}

void ClientRegistry::RegisterClient(MemoryCoordinatorClient* client) {
  clients_->AddObserver(client);
}

void ClientRegistry::UnregisterClient(MemoryCoordinatorClient* client) {
  clients_->RemoveObserver(client);
}

}  // namespace memory_coordinator

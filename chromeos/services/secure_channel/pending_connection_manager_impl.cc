// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_connection_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

// static
PendingConnectionManagerImpl::Factory*
    PendingConnectionManagerImpl::Factory::test_factory_ = nullptr;

// static
PendingConnectionManagerImpl::Factory*
PendingConnectionManagerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void PendingConnectionManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PendingConnectionManagerImpl::Factory::~Factory() = default;

std::unique_ptr<PendingConnectionManager>
PendingConnectionManagerImpl::Factory::BuildInstance(Delegate* delegate) {
  return base::WrapUnique(new PendingConnectionManagerImpl(delegate));
}

PendingConnectionManagerImpl::PendingConnectionManagerImpl(Delegate* delegate)
    : PendingConnectionManager(delegate) {}

PendingConnectionManagerImpl::~PendingConnectionManagerImpl() = default;

void PendingConnectionManagerImpl::HandleConnectionRequest(
    const ConnectionAttemptDetails& connection_attempt_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority) {
  NOTIMPLEMENTED();
}

}  // namespace secure_channel

}  // namespace chromeos

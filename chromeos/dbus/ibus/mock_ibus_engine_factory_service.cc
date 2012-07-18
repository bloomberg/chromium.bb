// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_engine_factory_service.h"

namespace chromeos {

MockIBusEngineFactoryService::MockIBusEngineFactoryService() {
}

MockIBusEngineFactoryService::~MockIBusEngineFactoryService() {
}

void MockIBusEngineFactoryService::SetCreateEngineHandler(
    const CreateEngineHandler& create_engine_handler) {
}

void MockIBusEngineFactoryService::UnsetCreateEngineHandler() {
}

}  // namespace chromeos

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_FACTORY_SERVICE_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_FACTORY_SERVICE_H_

#include "base/basictypes.h"
#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"

namespace chromeos {
class MockIBusEngineFactoryService : public IBusEngineFactoryService {
 public:
  MockIBusEngineFactoryService();
  virtual ~MockIBusEngineFactoryService();

  virtual void SetCreateEngineHandler(
      const CreateEngineHandler& create_engine_handler) OVERRIDE;
  virtual void UnsetCreateEngineHandler() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MockIBusEngineFactoryService);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_FACTORY_SERVICE_H_

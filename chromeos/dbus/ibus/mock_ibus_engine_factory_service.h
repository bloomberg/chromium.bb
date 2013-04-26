// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_FACTORY_SERVICE_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_FACTORY_SERVICE_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"
#include "dbus/object_path.h"

namespace chromeos {
class MockIBusEngineFactoryService : public IBusEngineFactoryService {
 public:
  MockIBusEngineFactoryService();
  virtual ~MockIBusEngineFactoryService();

  virtual void SetCreateEngineHandler(
      const std::string& engine_id,
      const CreateEngineHandler& create_engine_handler) OVERRIDE;
  virtual void UnsetCreateEngineHandler(
      const std::string& engine_id) OVERRIDE;

  bool CallCreateEngine(const std::string& engine_id);

  int set_create_engine_handler_call_count() const {
    return set_create_engine_handler_call_count_;
  }

  int unset_create_engine_handler_call_count() const {
    return unset_create_engine_handler_call_count_;
  }

  dbus::ObjectPath GetObjectPathByEngineId(const std::string& engine_id);

 private:
  void OnEngineCreated(const std::string& engine_id,
                       const dbus::ObjectPath& path);

  std::map<std::string, CreateEngineHandler> handler_map_;
  std::map<std::string, dbus::ObjectPath> object_path_map_;
  int set_create_engine_handler_call_count_;
  int unset_create_engine_handler_call_count_;
  base::WeakPtrFactory<MockIBusEngineFactoryService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockIBusEngineFactoryService);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_FACTORY_SERVICE_H_

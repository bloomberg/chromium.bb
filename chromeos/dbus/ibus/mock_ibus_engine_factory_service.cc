// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_engine_factory_service.h"

#include "base/bind.h"

namespace chromeos {

MockIBusEngineFactoryService::MockIBusEngineFactoryService()
    : set_create_engine_handler_call_count_(0),
      unset_create_engine_handler_call_count_(0),
      weak_ptr_factory_(this) {
}

MockIBusEngineFactoryService::~MockIBusEngineFactoryService() {
}

void MockIBusEngineFactoryService::SetCreateEngineHandler(
    const std::string& engine_id,
    const CreateEngineHandler& create_engine_handler) {
  handler_map_[engine_id] = create_engine_handler;
  set_create_engine_handler_call_count_++;
}

void MockIBusEngineFactoryService::UnsetCreateEngineHandler(
    const std::string& engine_id) {
  unset_create_engine_handler_call_count_++;
  handler_map_[engine_id].Reset();
}

bool MockIBusEngineFactoryService::CallCreateEngine(
    const std::string& engine_id) {
  if (handler_map_.find(engine_id) != handler_map_.end() &&
      !handler_map_[engine_id].is_null()) {
    handler_map_[engine_id].Run(
        base::Bind(&MockIBusEngineFactoryService::OnEngineCreated,
                   weak_ptr_factory_.GetWeakPtr(),
                   engine_id));
    return true;
  }
  return false;
}

dbus::ObjectPath MockIBusEngineFactoryService::GetObjectPathByEngineId(
    const std::string& engine_id) {
  if (object_path_map_.find(engine_id) != object_path_map_.end())
    return dbus::ObjectPath();
  return object_path_map_[engine_id];
}

void MockIBusEngineFactoryService::OnEngineCreated(
    const std::string& engine_id,
    const dbus::ObjectPath& path) {
  object_path_map_[engine_id] = path;
}

}  // namespace chromeos

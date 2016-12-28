// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/memory_handler.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/strings/stringprintf.h"
#include "content/browser/memory/memory_pressure_controller_impl.h"
#include "content/public/common/content_features.h"

namespace content {
namespace protocol {

MemoryHandler::MemoryHandler()
    : DevToolsDomainHandler(Memory::Metainfo::domainName) {
}

MemoryHandler::~MemoryHandler() {}

void MemoryHandler::Wire(UberDispatcher* dispatcher) {
  Memory::Dispatcher::wire(dispatcher, this);
}

Response MemoryHandler::SetPressureNotificationsSuppressed(
    bool suppressed) {
  if (base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
    return Response::Error(
        "Cannot enable/disable notifications when memory coordinator is "
        "enabled");
  }
  content::MemoryPressureControllerImpl::GetInstance()
      ->SetPressureNotificationsSuppressedInAllProcesses(suppressed);
  return Response::OK();
}

Response MemoryHandler::SimulatePressureNotification(
    const std::string& level) {
  base::MemoryPressureListener::MemoryPressureLevel parsed_level;
  if (level == protocol::Memory::PressureLevelEnum::Moderate) {
    parsed_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  } else if (level == protocol::Memory::PressureLevelEnum::Critical) {
    parsed_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  } else {
    return Response::InvalidParams(base::StringPrintf(
        "Invalid memory pressure level '%s'", level.c_str()));
  }

  MemoryPressureControllerImpl::GetInstance()
      ->SimulatePressureNotificationInAllProcesses(parsed_level);
  return Response::OK();
}

}  // namespace protocol
}  // namespace content

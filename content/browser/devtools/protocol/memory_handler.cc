// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/memory_handler.h"

#include "content/browser/memory/memory_pressure_controller.h"

namespace content {
namespace devtools {
namespace memory {

MemoryHandler::MemoryHandler() {}

MemoryHandler::~MemoryHandler() {}

MemoryHandler::Response MemoryHandler::SetPressureNotificationsSuppressed(
    bool suppressed) {
  content::MemoryPressureController::GetInstance()
      ->SetPressureNotificationsSuppressedInAllProcesses(suppressed);
  return Response::OK();
}

}  // namespace memory
}  // namespace devtools
}  // namespace content

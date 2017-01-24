// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/file_system/arc_file_system_operation_runner.h"

namespace arc {

// static
const char ArcFileSystemOperationRunner::kArcServiceName[] =
    "arc::ArcFileSystemOperationRunner";

ArcFileSystemOperationRunner::ArcFileSystemOperationRunner(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service) {}

ArcFileSystemOperationRunner::~ArcFileSystemOperationRunner() = default;

}  // namespace arc

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_process.h"

namespace arc {

ArcProcess::ArcProcess(base::ProcessId nspid,
                       base::ProcessId pid,
                       const std::string& process_name,
                       mojom::ProcessState process_state)
    : nspid_(nspid),
      pid_(pid),
      process_name_(process_name),
      process_state_(process_state) {}

ArcProcess::~ArcProcess() = default;

ArcProcess::ArcProcess(ArcProcess&& other) = default;
ArcProcess& ArcProcess::operator=(ArcProcess&& other) = default;

}  // namespace arc

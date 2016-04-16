// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_H_

#include <string>

#include "base/process/process_handle.h"
#include "components/arc/common/process.mojom.h"

namespace arc {

struct ArcProcess {
  base::ProcessId nspid;
  base::ProcessId pid;
  std::string process_name;
  mojom::ProcessState process_state;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_H_

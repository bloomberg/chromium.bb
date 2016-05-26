// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SERVICE_LAUNCHER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/arc_service_manager.h"

namespace arc {

// Detects ARC availability and launches ARC bridge service.
class ArcServiceLauncher {
 public:
  ArcServiceLauncher();
  ~ArcServiceLauncher();

  void Initialize();
  void Shutdown();

 private:
  // DBus callback.
  void OnArcAvailable(bool available);

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  base::WeakPtrFactory<ArcServiceLauncher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceLauncher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SERVICE_LAUNCHER_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_
#define CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "content/public/browser/resource_throttle.h"

namespace content {

class PowerSaveBlocker;

// This ResourceThrottle blocks power save until large upload request finishes.
class PowerSaveBlockResourceThrottle : public ResourceThrottle {
 public:
  explicit PowerSaveBlockResourceThrottle(const std::string& host);
  ~PowerSaveBlockResourceThrottle() override;

  // ResourceThrottle overrides:
  void WillStartRequest(bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;

 private:
  void ActivatePowerSaveBlocker();

  const std::string host_;
  base::OneShotTimer timer_;
  scoped_ptr<PowerSaveBlocker> power_save_blocker_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlockResourceThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_

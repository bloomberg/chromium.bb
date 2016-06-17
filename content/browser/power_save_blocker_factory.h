// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_SAVE_BLOCKER_FACTORY_H_
#define CONTENT_BROWSER_POWER_SAVE_BLOCKER_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "device/power_save_blocker/power_save_blocker.h"

namespace content {

CONTENT_EXPORT std::unique_ptr<device::PowerSaveBlocker> CreatePowerSaveBlocker(
    device::PowerSaveBlocker::PowerSaveBlockerType type,
    device::PowerSaveBlocker::Reason reason,
    const std::string& description);

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_SAVE_BLOCKER_FACTORY_H_

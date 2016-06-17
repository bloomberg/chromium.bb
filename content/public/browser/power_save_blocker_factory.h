// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_POWER_SAVE_BLOCKER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_POWER_SAVE_BLOCKER_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/browser/power_save_blocker.h"

namespace content {

CONTENT_EXPORT std::unique_ptr<PowerSaveBlocker> CreatePowerSaveBlocker(
    PowerSaveBlocker::PowerSaveBlockerType type,
    PowerSaveBlocker::Reason reason,
    const std::string& description);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_POWER_SAVE_BLOCKER_FACTORY_H_

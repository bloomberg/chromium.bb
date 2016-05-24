// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include "build/build_config.h"

namespace content {

PowerSaveBlocker::~PowerSaveBlocker() {}

// static
std::unique_ptr<PowerSaveBlocker> PowerSaveBlocker::CreateWithTaskRunners(
    PowerSaveBlocker::PowerSaveBlockerType type,
    PowerSaveBlocker::Reason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  return std::unique_ptr<PowerSaveBlocker>(new PowerSaveBlockerImpl(
      type, reason, description, ui_task_runner, blocking_task_runner));
}

}  // namespace content

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_client_config.h"

#include "base/task_scheduler/post_task.h"

namespace extensions {

UpdateClientConfig::UpdateClientConfig() {}

scoped_refptr<base::SequencedTaskRunner>
UpdateClientConfig::GetSequencedTaskRunner() const {
  return base::CreateSequencedTaskRunnerWithTraits(
      base::TaskTraits()
          .MayBlock()
          .WithPriority(base::TaskPriority::BACKGROUND)
          .WithShutdownBehavior(base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN));
}

UpdateClientConfig::~UpdateClientConfig() {}

}  // namespace extensions

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_client_config.h"

#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "build/build_config.h"

namespace extensions {

UpdateClientConfig::UpdateClientConfig() {}

scoped_refptr<base::SequencedTaskRunner>
UpdateClientConfig::GetSequencedTaskRunner() const {
  constexpr base::TaskTraits traits = {
      base::MayBlock(), base::TaskPriority::BACKGROUND,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
#if defined(OS_WIN)
  // Use the COM STA task runner as the Windows background downloader requires
  // COM initialization.
  return base::CreateCOMSTATaskRunnerWithTraits(traits);
#else
  return base::CreateSequencedTaskRunnerWithTraits(traits);
#endif
}

UpdateClientConfig::~UpdateClientConfig() {}

}  // namespace extensions

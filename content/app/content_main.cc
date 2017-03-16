// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main.h"

#include <memory>

#include "base/debug/activity_tracker.h"
#include "content/public/app/content_main_runner.h"

namespace content {

int ContentMain(const ContentMainParams& params) {
  std::unique_ptr<ContentMainRunner> main_runner(ContentMainRunner::Create());

  int exit_code = main_runner->Initialize(params);
  if (exit_code >= 0) {
    base::debug::GlobalActivityTracker* tracker =
        base::debug::GlobalActivityTracker::Get();
    if (tracker) {
      tracker->SetProcessPhase(
          base::debug::GlobalActivityTracker::PROCESS_LAUNCH_FAILED);
      tracker->process_data().SetInt("exit-code", exit_code);
    }
    return exit_code;
  }

  exit_code = main_runner->Run();

  main_runner->Shutdown();

  base::debug::GlobalActivityTracker* tracker =
      base::debug::GlobalActivityTracker::Get();
  if (tracker) {
    if (exit_code == 0) {
      tracker->SetProcessPhaseIfEnabled(
          base::debug::GlobalActivityTracker::PROCESS_EXITED_CLEANLY);
    } else {
      tracker->SetProcessPhaseIfEnabled(
          base::debug::GlobalActivityTracker::PROCESS_EXITED_WITH_CODE);
      tracker->process_data().SetInt("exit-code", exit_code);
    }
  }

  return exit_code;
}

}  // namespace content

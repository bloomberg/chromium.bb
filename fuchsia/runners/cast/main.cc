// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/default_context.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "fuchsia/runners/cast/cast_runner.h"

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;

  constexpr fuchsia::web::ContextFeatureFlags kCastRunnerFeatures =
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::VULKAN |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER;

  CastRunner runner(
      base::fuchsia::ComponentContextForCurrentProcess()->outgoing().get(),
      WebContentRunner::CreateIncognitoWebContext(kCastRunnerFeatures));

  base::fuchsia::ComponentContextForCurrentProcess()
      ->outgoing()
      ->ServeFromStartupInfo();

  // Run until there are no Components, or the last service client channel is
  // closed.
  // TODO(https://crbug.com/952560): Implement Components v2 graceful exit.
  run_loop.Run();

  return 0;
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/installer/zucchini/main_utils.h"

namespace {

void InitLogging() {
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  settings.log_file = nullptr;
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
  bool logging_res = logging::InitLogging(settings);
  CHECK(logging_res);
}

}  // namespace

int main(int argc, const char* argv[]) {
  ResourceUsageTracker tracker;
  base::CommandLine::Init(argc, argv);
  InitLogging();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Instantiate Command registry and register Zucchini features.
  CommandRegistry registry;
  registry.Register(&kCommandGen);
  registry.Register(&kCommandApply);

  registry.RunOrExit(command_line);
  return 0;
}

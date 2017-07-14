// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/main_utils.h"

#include <iostream>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "chrome/installer/zucchini/io_utils.h"

namespace {

// Translates |command_line| arguments to a vector of base::FilePath and returns
// the result via |fnames|. Expects exactly |expected_count|.
bool CheckAndGetFilePathParams(const base::CommandLine& command_line,
                               size_t expected_count,
                               std::vector<base::FilePath>* fnames) {
  const base::CommandLine::StringVector& args = command_line.GetArgs();
  if (args.size() != expected_count)
    return false;

  fnames->clear();
  for (size_t i = 0; i < args.size(); ++i)
    fnames->push_back(base::FilePath(args[i]));
  return true;
}

}  // namespace

/******** ResourceUsageTracker ********/

ResourceUsageTracker::ResourceUsageTracker() : start_time_(base::Time::Now()) {}

ResourceUsageTracker::~ResourceUsageTracker() {
  base::Time end_time = base::Time::Now();

#if !defined(OS_MACOSX)
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));

  LOG(INFO) << "Zucchini.PeakPagefileUsage "
            << process_metrics->GetPeakPagefileUsage() / 1024 << " KiB";
  LOG(INFO) << "Zucchini.PeakWorkingSetSize "
            << process_metrics->GetPeakWorkingSetSize() / 1024 << " KiB";
#endif  // !defined(OS_MACOSX)

  LOG(INFO) << "Zucchini.TotalTime " << (end_time - start_time_).InSecondsF()
            << " s";
}

/******** Command ********/

Command::Command(const char* name_in,
                 const char* usage_in,
                 int num_args_in,
                 Command::Fun fun_in)
    : name(name_in), usage(usage_in), num_args(num_args_in), fun(fun_in) {}

Command::Command(const Command&) = default;

Command::~Command() = default;

/******** CommandRegistry ********/

CommandRegistry::CommandRegistry() = default;

CommandRegistry::~CommandRegistry() = default;

void CommandRegistry::Register(const Command* command) {
  commands_.push_back(command);
}

void CommandRegistry::RunOrExit(const base::CommandLine& command_line) {
  const Command* command_use = nullptr;
  for (const Command* command : commands_) {
    if (command_line.HasSwitch(command->name)) {
      if (command_use) {        // Too many commands found.
        command_use = nullptr;  // Set to null to flag error.
        break;
      }
      command_use = command;
    }
  }

  // If we don't have exactly one matching command, print error and exit.
  if (!command_use) {
    std::cerr << "Must have exactly one of:\n  [";
    zucchini::PrefixSep sep(", ");
    for (const Command* command : commands_)
      std::cerr << sep << "-" << command->name;
    std::cerr << "]" << std::endl;
    PrintUsageAndExit();
  }

  std::vector<base::FilePath> fnames;
  if (CheckAndGetFilePathParams(command_line, command_use->num_args, &fnames)) {
    command_use->fun.Run(command_line, fnames);
  } else {
    std::cerr << command_use->usage << std::endl;
    PrintUsageAndExit();
  }
}

void CommandRegistry::PrintUsageAndExit() {
  std::cerr << "Usage:" << std::endl;
  for (const Command* command : commands_)
    std::cerr << "  zucchini " << command->usage << std::endl;
  exit(1);
}

/******** Command Definitions ********/

Command kCommandGen = {
    "gen", "-gen <old_file> <new_file> <patch_file>", 3,
    base::Bind([](const base::CommandLine& command_line,
                  const std::vector<base::FilePath>& fnames) -> void {
      // TODO(etiennep): Implement.
    })};

Command kCommandApply = {
    "apply", "-apply <old_file> <patch_file> <new_file>", 3,
    base::Bind([](const base::CommandLine& command_line,
                  const std::vector<base::FilePath>& fnames) -> void {
      // TODO(etiennep): Implement.
    })};

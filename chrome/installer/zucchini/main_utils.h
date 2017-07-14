// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_MAIN_UTILS_H_
#define CHROME_INSTALLER_ZUCCHINI_MAIN_UTILS_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace base {

class CommandLine;
class FilePath;

}  // namespace base

// Class to track and print system resource usage. Should be instantiated early
// in program flow to better track start time.
class ResourceUsageTracker {
 public:
  ResourceUsageTracker();
  ~ResourceUsageTracker();

 private:
  base::Time start_time_;

  DISALLOW_COPY_AND_ASSIGN(ResourceUsageTracker);
};

// Specs for a Zucchini command.
struct Command {
  using Fun = base::Callback<void(const base::CommandLine&,
                                  const std::vector<base::FilePath>&)>;

  Command(const char* name_in,
          const char* usage_in,
          int num_args_in,
          Fun fun_in);
  explicit Command(const Command&);
  ~Command();

  // Unique name of command. |-name| is used to select from command line.
  const char* name;

  // Usage help text of command.
  const char* usage;

  // Number of arguments (assumed to be filenames) used by the command.
  const int num_args;

  // Main code for the command.
  Fun fun;
};

// Registry of Commands to select the command to run and to handle errors.
class CommandRegistry {
 public:
  CommandRegistry();
  ~CommandRegistry();

  void Register(const Command* command);

  // Uses |command_line| to find a Command instance. If a unique command is
  // found, then runs it. Otherwise prints error and exits.
  void RunOrExit(const base::CommandLine& command_line);

 private:
  void PrintUsageAndExit();

  std::vector<const Command*> commands_;

  DISALLOW_COPY_AND_ASSIGN(CommandRegistry);
};

// Command: Patch generation.
extern Command kCommandGen;

// Command: Patch application.
extern Command kCommandApply;

#endif  // CHROME_INSTALLER_ZUCCHINI_MAIN_UTILS_H_

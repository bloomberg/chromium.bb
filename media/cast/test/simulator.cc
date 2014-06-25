// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simulation program.
// Input:
// - File path to writing out the raw event log of the simulation session.
// - Simulation parameters.
// - Unique simulation run ID for tagging the log.
// Output:
// - Raw event log of the simulation session tagged with the unique test ID,
//   written out to the specified file path.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"

const char kSimulationRunId[] = "simulation-run-id";
const char kOutputPath[] = "output-path";

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  const CommandLine* cmd = CommandLine::ForCurrentProcess();
  base::FilePath output_path = cmd->GetSwitchValuePath(kOutputPath);
  CHECK(!output_path.empty());
  std::string sim_run_id = cmd->GetSwitchValueASCII(kSimulationRunId);

  std::string msg = "Log from simulation run " + sim_run_id;
  int ret = base::WriteFile(output_path, &msg[0], msg.size());
  if (ret != static_cast<int>(msg.size()))
    VLOG(0) << "Failed to write logs to file.";

  return 0;
}

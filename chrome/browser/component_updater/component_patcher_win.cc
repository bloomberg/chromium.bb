// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_patcher_win.h"

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/util_constants.h"

namespace {

std::string PatchTypeToCommandLineSwitch(
    ComponentPatcher::PatchType patch_type) {
  if (patch_type == ComponentPatcher::kPatchTypeCourgette)
    return std::string(installer::kCourgette);
  else if (patch_type == ComponentPatcher::kPatchTypeBsdiff)
    return std::string(installer::kBsdiff);
  else
    return std::string();
}

// Finds the path to the setup.exe. First, it looks for the program in the
// "installer" directory. If the program is not found there, it tries to find it
// in the directory where chrome.dll lives. Returns the path to the setup.exe,
// if the path exists, otherwise it returns an an empty path.
base::FilePath FindSetupProgram() {
  base::FilePath exe_dir;
  if (!PathService::Get(base::DIR_MODULE, &exe_dir))
    return base::FilePath();

  const std::string installer_dir(WideToASCII(installer::kInstallerDir));
  const std::string setup_exe(WideToASCII(installer::kSetupExe));

  base::FilePath setup_path = exe_dir;
  setup_path = setup_path.AppendASCII(installer_dir);
  setup_path = setup_path.AppendASCII(setup_exe);
  if (base::PathExists(setup_path))
    return setup_path;

  setup_path = exe_dir;
  setup_path = setup_path.AppendASCII(setup_exe);
  if (base::PathExists(setup_path))
    return setup_path;

  return base::FilePath();
}

}  // namespace

// Applies the patch to the input file. Returns kNone if the patch was
// successfully applied, kDeltaOperationFailure if the patch operation
// encountered errors, and kDeltaPatchProcessFailure if there was an error
// when running the patch code out of process. In the error case, detailed error
// information could be returned in the error parameter.
ComponentUnpacker::Error ComponentPatcherWin::Patch(
    PatchType patch_type,
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file,
    int* error) {
  *error = 0;

  const base::FilePath exe_path = FindSetupProgram();
  if (exe_path.empty())
    return ComponentUnpacker::kDeltaPatchProcessFailure;

  const std::string patch_type_str(PatchTypeToCommandLineSwitch(patch_type));

  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitchASCII(installer::switches::kPatch, patch_type_str.c_str());
  cl.AppendSwitchPath(installer::switches::kInputFile, input_file);
  cl.AppendSwitchPath(installer::switches::kPatchFile, patch_file);
  cl.AppendSwitchPath(installer::switches::kOutputFile, output_file);

  // Create the child process in a job object. The job object prevents leaving
  // child processes around when the parent process exits, either gracefully or
  // accidentally.
  base::win::ScopedHandle job(CreateJobObject(NULL, NULL));
  if (!job ||
      !base::SetJobObjectLimitFlags(job, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE)) {
    *error = GetLastError();
    return ComponentUnpacker::kDeltaPatchProcessFailure;
  }

  base::LaunchOptions launch_options;
  launch_options.wait = true;
  launch_options.job_handle = job;
  launch_options.start_hidden = true;
  CommandLine setup_path(exe_path);
  setup_path.AppendArguments(cl, false);

  // |ph| is closed by WaitForExitCode.
  base::ProcessHandle ph = base::kNullProcessHandle;
  int exit_code = 0;
  if (!base::LaunchProcess(setup_path, launch_options, &ph) ||
      !base::WaitForExitCode(ph, &exit_code)) {
    *error = GetLastError();
    return ComponentUnpacker::kDeltaPatchProcessFailure;
  }

  *error = exit_code;
  return *error ? ComponentUnpacker::kDeltaOperationFailure :
                  ComponentUnpacker::kNone;
}


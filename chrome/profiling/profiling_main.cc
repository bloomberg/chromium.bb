// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_main.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/profiling/profiling_globals.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace profiling {

namespace {

base::CommandLine MakeBrowserCommandLine(const base::CommandLine& cmdline,
                                         const std::string& pipe_id) {
  const base::CommandLine::StringVector& our_argv = cmdline.argv();

  base::CommandLine::StringVector browser_argv;
  browser_argv.reserve(our_argv.size());

  // Program name.
  base::FilePath child_path;
#if defined(OS_LINUX)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  // When running under Valgrind, forking /proc/self/exe ends up forking the
  // Valgrind executable, which then crashes. However, it's almost safe to
  // assume that the updates won't happen while testing with Valgrind tools.
  if (!RunningOnValgrind())
    child_path = base::FilePath(base::kProcSelfExe);
#endif

  if (child_path.empty())
    base::PathService::Get(base::FILE_EXE, &child_path);
  browser_argv.push_back(child_path.value());  // Program name.

  // Remove all memlog flags.
  for (size_t i = 1; i < our_argv.size(); i++) {
    if (!base::StartsWith(our_argv[i], FILE_PATH_LITERAL("--memlog"),
                          base::CompareCase::SENSITIVE))
      browser_argv.push_back(our_argv[i]);
  }

  // Append the pipe ID.
  std::string pipe_switch("--");
  pipe_switch.append(switches::kMemlogPipe);
  pipe_switch.push_back('=');
  pipe_switch.append(pipe_id);
#if defined(OS_WIN)
  browser_argv.push_back(base::ASCIIToUTF16(pipe_switch));
#else
  browser_argv.push_back(pipe_switch);
#endif

  return base::CommandLine(browser_argv);
}

bool LaunchBrowser(const base::CommandLine& our_cmdline,
                   const std::string& pipe_id) {
  base::CommandLine browser_cmdline =
      MakeBrowserCommandLine(our_cmdline, pipe_id);

  base::LaunchOptions options;
  base::Process process = base::LaunchProcess(browser_cmdline, options);

  return true;
}

}  // namespace

int ProfilingMain(const base::CommandLine& cmdline) {
  ProfilingGlobals* globals = ProfilingGlobals::Get();

  mojo::edk::Init();
  mojo::edk::ScopedIPCSupport ipc_support(
      globals->GetIORunner(),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  base::Process process = base::Process::Current();
  std::string pipe_id = base::IntToString(static_cast<int>(process.Pid()));

  if (!LaunchBrowser(cmdline, pipe_id))
    return 1;

  ProfilingGlobals::Get()->RunMainMessageLoop();

#if defined(OS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif
  return 0;
}

}  // namespace profiling

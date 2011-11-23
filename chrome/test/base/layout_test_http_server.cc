// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/layout_test_http_server.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

bool PrepareCommandLine(CommandLine* cmd_line) {
  FilePath src_path;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_path))
    return false;

  cmd_line->SetProgram(FilePath(FILE_PATH_LITERAL("python")));

  FilePath script_path(src_path);
  script_path = script_path.AppendASCII("third_party");
  script_path = script_path.AppendASCII("WebKit");
  script_path = script_path.AppendASCII("Tools");
  script_path = script_path.AppendASCII("Scripts");
  script_path = script_path.AppendASCII("new-run-webkit-httpd");

  cmd_line->AppendArgPath(script_path);
  return true;
}

}  // namespace

LayoutTestHttpServer::LayoutTestHttpServer(const FilePath& root_directory,
                                           int port)
    : root_directory_(root_directory),
      port_(port),
      running_(false) {
}

LayoutTestHttpServer::~LayoutTestHttpServer() {
  if (running_ && !Stop())
    LOG(ERROR) << "LayoutTestHttpServer failed to stop.";
}

bool LayoutTestHttpServer::Start() {
  if (running_) {
    LOG(ERROR) << "LayoutTestHttpServer already running.";
    return false;
  }

  CommandLine cmd_line(CommandLine::NO_PROGRAM);
  if (!PrepareCommandLine(&cmd_line))
    return false;
  cmd_line.AppendArg("--server=start");
  cmd_line.AppendArg("--register_cygwin");
  cmd_line.AppendArgNative(FILE_PATH_LITERAL("--root=") +
                           root_directory_.value());
  cmd_line.AppendArg("--port=" + base::IntToString(port_));

  FilePath layout_tests_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &layout_tests_dir))
    return false;
  layout_tests_dir = layout_tests_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("layout_tests")
                                     .AppendASCII("LayoutTests");
  cmd_line.AppendArgNative(FILE_PATH_LITERAL("--layout_tests_dir=") +
                           layout_tests_dir.value());

#if defined(OS_WIN)
  // For Windows 7, if we start the lighttpd server on the foreground mode,
  // it will mess up with the command window and cause conhost.exe to crash. To
  // work around this, we start the http server on the background mode.
  if (base::win::GetVersion() >= base::win::VERSION_WIN7)
    cmd_line.AppendArg("--run_background");

  job_handle_.Set(CreateJobObject(NULL, NULL));
  if (!job_handle_.IsValid()) {
    LOG(ERROR) << "Could not create JobObject.";
    return false;
  }

  if (!base::SetJobObjectAsKillOnJobClose(job_handle_.Get())) {
    LOG(ERROR) << "Could not SetInformationJobObject.";
    return false;
  }
#endif

  // The Python script waits for the server to start responding to requests,
  // then exits.  So we want to wait for the Python script to exit before
  // continuing.
  base::LaunchOptions options;
  options.wait = true;
#if defined(OS_WIN)
  options.job_handle = job_handle_.Get();
#endif
  running_ = base::LaunchProcess(cmd_line, options, NULL);
  return running_;
}

bool LayoutTestHttpServer::Stop() {
  if (!running_) {
    LOG(ERROR) << "LayoutTestHttpServer not running.";
    return false;
  }

  CommandLine cmd_line(CommandLine::NO_PROGRAM);
  if (!PrepareCommandLine(&cmd_line))
    return false;
  cmd_line.AppendArg("--server=stop");

  base::LaunchOptions options;
  options.wait = true;
#if defined(OS_WIN)
  options.job_handle = job_handle_.Get();
#endif
  bool stopped = base::LaunchProcess(cmd_line, options, NULL);
  running_ = !stopped;

#if defined(OS_WIN)
  // Close the job object handle now. This should clean up
  // any orphaned processes.
  job_handle_.Close();
#endif

  return stopped;
}

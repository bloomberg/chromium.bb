// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/boot_times_loader.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"

namespace chromeos {

// Beginning of line we look for that gives version number.
static const char kPrefix[] = "CHROMEOS_RELEASE_DESCRIPTION=";

// File to look for version number in.
static const char kVersionPath[] = "/etc/lsb-release";

// File uptime logs are located in.
static const char kLogPath[] = "/tmp";

BootTimesLoader::BootTimesLoader() : backend_(new Backend()) {
}

BootTimesLoader::Handle BootTimesLoader::GetBootTimes(
    CancelableRequestConsumerBase* consumer,
    BootTimesLoader::GetBootTimesCallback* callback) {
  if (!g_browser_process->file_thread()) {
    // This should only happen if Chrome is shutting down, so we don't do
    // anything.
    return 0;
  }

  scoped_refptr<CancelableRequest<GetBootTimesCallback> > request(
      new CancelableRequest<GetBootTimesCallback>(callback));
  AddRequest(request, consumer);

  g_browser_process->file_thread()->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(backend_.get(), &Backend::GetBootTimes, request));
  return request->handle();
}

// Executes command within a shell, allowing IO redirection. Searches
// for a whitespace delimited string prefixed by prefix in the output and
// returns it.
static std::string ExecuteInShell(
    const std::string& command, const std::string& prefix) {
  std::vector<std::string> args;
  args.push_back("/bin/sh");
  args.push_back("-c");
  args.push_back(command);
  CommandLine cmd(args);
  std::string out;

  if (base::GetAppOutput(cmd, &out)) {
    size_t index = out.find(prefix);
    if (index != std::string::npos) {
      size_t value_index = index + prefix.size();
      size_t whitespace_index = out.find(' ', value_index);
      size_t chars_left = std::string::npos;
      if (whitespace_index == std::string::npos)
        whitespace_index = out.find('\n', value_index);
      if (whitespace_index != std::string::npos)
        chars_left = whitespace_index - value_index;
      return out.substr(value_index, chars_left);
    }
  }
  return std::string();
}

// Extracts the uptime value from files located in /tmp, returning the
// value as a double in value.
static bool GetUptime(const std::string& log, double* value) {
  FilePath log_dir(kLogPath);
  FilePath log_file = log_dir.Append(log);
  std::string contents;
  *value = 0.0;
  if (file_util::ReadFileToString(log_file, &contents)) {
    size_t space_index = contents.find(' ');
    size_t chars_left =
        space_index != std::string::npos ? space_index : std::string::npos;
    std::string value_string = contents.substr(0, chars_left);
    return StringToDouble(value_string, value);
  }
  return false;
}

void BootTimesLoader::Backend::GetBootTimes(
    scoped_refptr<GetBootTimesRequest> request) {
  const char* kInitialTSCCommand = "dmesg | grep 'Initial TSC value:'";
  const char* kInitialTSCPrefix = "TSC value: ";
  const char* kClockSpeedCommand = "dmesg | grep -e 'Detected.*processor'";
  const char* kClockSpeedPrefix = "Detected ";
  const char* kPreStartup = "uptime-pre-startup";
  const char* kChromeExec = "uptime-chrome-exec";
  const char* kChromeMain = "uptime-chrome-main";
  const char* kXStarted = "uptime-x-started";
  const char* kLoginPromptReady = "uptime-login-prompt-ready";

  if (request->canceled())
    return;

  // Wait until login_prompt_ready is output.
  FilePath log_dir(kLogPath);
  FilePath log_file = log_dir.Append(kLoginPromptReady);
  while (!file_util::PathExists(log_file)) {
    usleep(500000);
  }

  BootTimes boot_times;
  std::string tsc_value = ExecuteInShell(kInitialTSCCommand, kInitialTSCPrefix);
  std::string speed_value =
      ExecuteInShell(kClockSpeedCommand, kClockSpeedPrefix);
  boot_times.firmware = 0;
  if (!tsc_value.empty() && !speed_value.empty()) {
    int64 tsc = 0;
    double speed = 0;
    if (StringToInt64(tsc_value, &tsc) &&
        StringToDouble(speed_value, &speed) &&
        speed > 0) {
      boot_times.firmware = static_cast<double>(tsc) / (speed * 1000000);
    }
  }
  GetUptime(kPreStartup, &boot_times.pre_startup);
  GetUptime(kXStarted, &boot_times.x_started);
  GetUptime(kChromeExec, &boot_times.chrome_exec);
  GetUptime(kChromeMain, &boot_times.chrome_main);
  GetUptime(kLoginPromptReady, &boot_times.login_prompt_ready);

  request->ForwardResult(
      GetBootTimesCallback::TupleType(request->handle(), boot_times));
}

}  // namespace chromeos

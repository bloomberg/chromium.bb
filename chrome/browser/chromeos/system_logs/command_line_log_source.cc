// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/command_line_log_source.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Gathers log data from various scripts/programs.
void ExecuteCommandLines(system_logs::SystemLogsResponse* response) {
  // TODO(tudalex): Move program calling in a array or something similar to make
  // it more easier to modify and understand.
  std::vector<std::pair<std::string, base::CommandLine>> commands;

  base::CommandLine command(base::FilePath("/usr/bin/amixer"));
  command.AppendArg("-c0");
  command.AppendArg("contents");
  commands.push_back(std::make_pair("alsa controls", command));

  command = base::CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command.AppendArg("--dump_server_info");
  command.AppendArg("--dump_audio_thread");
  commands.push_back(std::make_pair("cras", command));

  command = base::CommandLine((base::FilePath("/usr/bin/audio_diagnostics")));
  commands.push_back(std::make_pair("audio_diagnostics", command));

#if 0
  // This command hangs as of R39. TODO(alhli): Make cras_test_client more
  // robust or add a wrapper script that times out, and fix this or remove
  // this code. crbug.com/419523
  command = base::CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command.AppendArg("--loopback_file");
  command.AppendArg("/dev/null");
  command.AppendArg("--rate");
  command.AppendArg("44100");
  command.AppendArg("--duration_seconds");
  command.AppendArg("0.01");
  command.AppendArg("--show_total_rms");
  commands.push_back(std::make_pair("cras_rms", command));
#endif

  command = base::CommandLine((base::FilePath("/usr/bin/printenv")));
  commands.push_back(std::make_pair("env", command));

#if defined(USE_X11)
  command = base::CommandLine(base::FilePath("/usr/bin/xrandr"));
  command.AppendArg("--verbose");
  commands.push_back(std::make_pair("xrandr", command));
#elif defined(USE_OZONE)
  command = base::CommandLine(base::FilePath("/usr/bin/modetest"));
  commands.push_back(std::make_pair("modetest", command));
#endif

  // Get a list of file sizes for the whole system (excluding the names of the
  // files in the Downloads directory for privay reasons).
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // The following command would hang if run in Linux Chrome OS build on a
    // Linux Workstation.
    command = base::CommandLine(base::FilePath("/bin/sh"));
    command.AppendArg("-c");
    command.AppendArg(
        "/usr/bin/du -h / | grep -v -e \\/home\\/.*\\/Downloads\\/");
    commands.push_back(std::make_pair("system_files", command));
  }

  // Get disk space usage information
  command = base::CommandLine(base::FilePath("/bin/df"));
  commands.push_back(std::make_pair("disk_usage", command));

  for (size_t i = 0; i < commands.size(); ++i) {
    VLOG(1) << "Executting System Logs Command: " << commands[i].first;
    std::string output;
    base::GetAppOutput(commands[i].second, &output);
    (*response)[commands[i].first] = output;
  }
}

}  // namespace

namespace system_logs {

CommandLineLogSource::CommandLineLogSource() : SystemLogsSource("CommandLine") {
}

CommandLineLogSource::~CommandLineLogSource() {
}

void CommandLineLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  SystemLogsResponse* response = new SystemLogsResponse;
  base::PostTaskWithTraitsAndReply(FROM_HERE,
                                   base::TaskTraits().MayBlock().WithPriority(
                                       base::TaskPriority::BACKGROUND),
                                   base::Bind(&ExecuteCommandLines, response),
                                   base::Bind(callback, base::Owned(response)));
}

}  // namespace system_logs

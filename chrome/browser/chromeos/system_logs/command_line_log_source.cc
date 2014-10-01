// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/command_line_log_source.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Gathers log data from various scripts/programs.
void ExecuteCommandLines(system_logs::SystemLogsResponse* response) {
  // TODO(tudalex): Move program calling in a array or something similar to make
  // it more easier to modify and understand.
  std::vector<std::pair<std::string, CommandLine> > commands;

  CommandLine command(base::FilePath("/usr/bin/amixer"));
  command.AppendArg("-c0");
  command.AppendArg("contents");
  commands.push_back(std::make_pair("alsa controls", command));

  command = CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command.AppendArg("--dump_server_info");
  command.AppendArg("--dump_audio_thread");
  commands.push_back(std::make_pair("cras", command));

  command = CommandLine((base::FilePath("/usr/bin/audio_diagnostics")));
  commands.push_back(std::make_pair("audio_diagnostics", command));

#if 0
  // This command hangs as of R39. TODO(alhli): Make cras_test_client more
  // robust or add a wrapper script that times out, and fix this or remove
  // this code. crbug.com/419523
  command = CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command.AppendArg("--loopback_file");
  command.AppendArg("/dev/null");
  command.AppendArg("--rate");
  command.AppendArg("44100");
  command.AppendArg("--duration_seconds");
  command.AppendArg("0.01");
  command.AppendArg("--show_total_rms");
  commands.push_back(std::make_pair("cras_rms", command));
#endif

  command = CommandLine((base::FilePath("/usr/bin/printenv")));
  commands.push_back(std::make_pair("env", command));

  command = CommandLine(base::FilePath("/usr/bin/setxkbmap"));
  command.AppendArg("-print");
  command.AppendArg("-query");
  commands.push_back(std::make_pair("setxkbmap", command));

  command = CommandLine(base::FilePath("/usr/bin/xinput"));
  command.AppendArg("list");
  command.AppendArg("--long");
  commands.push_back(std::make_pair("xinput", command));

  command = CommandLine(base::FilePath("/usr/bin/xrandr"));
  command.AppendArg("--verbose");
  commands.push_back(std::make_pair("xrandr", command));

  // Get a list of file sizes for the logged in user (excluding the names of
  // the files in the Downloads directory for privay reasons).
  command = CommandLine(base::FilePath("/bin/sh"));
  command.AppendArg("-c");
  command.AppendArg("/usr/bin/du -h /home/chronos/user |"
                    " grep -v -e \\/home\\/chronos\\/user\\/Downloads\\/");
  commands.push_back(std::make_pair("user_files", command));

  for (size_t i = 0; i < commands.size(); ++i) {
    std::string output;
    base::GetAppOutput(commands[i].second, &output);
    (*response)[commands[i].first] = output;
  }
}

}  // namespace

namespace system_logs {

void CommandLineLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse* response = new SystemLogsResponse;
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&ExecuteCommandLines, response),
      base::Bind(callback, base::Owned(response)));
}

}  // namespace system_logs

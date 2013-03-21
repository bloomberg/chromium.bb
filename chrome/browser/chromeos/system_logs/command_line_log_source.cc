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
#include "base/process_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Gathers log data from various scripts/programs.
void ExecuteCommandLines(chromeos::SystemLogsResponse* response) {
  // TODO(tudalex): Move program calling in a array or something similar to make
  // it more easier to modify and understand.
  std::vector<std::pair<std::string, CommandLine> > commands;

  CommandLine command(base::FilePath("/usr/bin/amixer"));
  command.AppendArg("-c0");
  command.AppendArg("contents");
  commands.push_back(std::make_pair("alsa controls", command));

  command = CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command.AppendArg("--dump_server_info");
  commands.push_back(std::make_pair("cras", command));

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

  command = CommandLine(base::FilePath("/opt/google/touchpad/tpcontrol"));
  command.AppendArg("status");
  commands.push_back(std::make_pair("hack-33025-touchpad", command));

  command =
      CommandLine(base::FilePath("/opt/google/touchpad/generate_userfeedback"));
  commands.push_back(std::make_pair("hack-33025-touchpad_activity", command));

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

namespace chromeos {

void CommandLineLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse* response = new SystemLogsResponse;
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&ExecuteCommandLines, response),
      base::Bind(callback, base::Owned(response)));
}

}  // namespace chromeos

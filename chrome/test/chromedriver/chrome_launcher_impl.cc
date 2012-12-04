// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome_launcher_impl.h"

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/chrome_finder.h"
#include "chrome/test/chromedriver/chrome_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"

ChromeLauncherImpl::ChromeLauncherImpl(URLRequestContextGetter* context_getter)
    : context_getter_(context_getter) {}

ChromeLauncherImpl::~ChromeLauncherImpl() {}

Status ChromeLauncherImpl::Launch(
    const FilePath& chrome_exe,
    scoped_ptr<Chrome>* chrome) {
  FilePath program = chrome_exe;
  if (program.empty()) {
    if (!FindChrome(&program))
      return Status(kUnknownError, "cannot find Chrome binary");
  }

  int port = 33081;
  CommandLine command(program);
  command.AppendSwitchASCII("remote-debugging-port", base::IntToString(port));
  command.AppendSwitch("no-first-run");
  command.AppendSwitch("enable-logging");
  command.AppendSwitchASCII("logging-level", "1");
  base::ScopedTempDir user_data_dir;
  if (!user_data_dir.CreateUniqueTempDir())
    return Status(kUnknownError, "cannot create temp dir for user data dir");
  command.AppendSwitchPath("user-data-dir", user_data_dir.path());
  command.AppendArg("about:blank");

  base::LaunchOptions options;
  base::ProcessHandle process;
  if (!base::LaunchProcess(command, options, &process))
    return Status(kUnknownError, "chrome failed to start");
  scoped_ptr<ChromeImpl> chrome_impl(new ChromeImpl(
      process, context_getter_, &user_data_dir, port));
  Status status = chrome_impl->Init();
  if (status.IsError())
    return status;
  chrome->reset(chrome_impl.release());
  return Status(kOk);
}

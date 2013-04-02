// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/sys_info.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

ChromeDesktopImpl::ChromeDesktopImpl(
    scoped_ptr<DevToolsHttpClient> client,
    const std::string& version,
    int build_no,
    base::ProcessHandle process,
    base::ScopedTempDir* user_data_dir,
    base::ScopedTempDir* extension_dir)
    : ChromeImpl(client.Pass(), version, build_no),
      process_(process) {
  if (user_data_dir->IsValid())
    CHECK(user_data_dir_.Set(user_data_dir->Take()));
  if (extension_dir->IsValid())
    CHECK(extension_dir_.Set(extension_dir->Take()));
}

ChromeDesktopImpl::~ChromeDesktopImpl() {
  base::CloseProcessHandle(process_);
}

std::string ChromeDesktopImpl::GetOperatingSystemName() {
  return base::SysInfo::OperatingSystemName();
}

Status ChromeDesktopImpl::Quit() {
  if (!base::KillProcess(process_, 0, true)) {
    int exit_code;
    if (base::GetTerminationStatus(process_, &exit_code) ==
        base::TERMINATION_STATUS_STILL_RUNNING)
      return Status(kUnknownError, "cannot kill Chrome");
  }
  return Status(kOk);
}

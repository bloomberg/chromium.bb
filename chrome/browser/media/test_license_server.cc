// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/test_license_server.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "chrome/browser/media/test_license_server_config.h"


TestLicenseServer::TestLicenseServer(
  scoped_ptr<TestLicenseServerConfig> server_config)
    : server_config_(server_config.Pass()),
      license_server_process_(base::kNullProcessHandle) {
}

TestLicenseServer::~TestLicenseServer() {
  Stop();
}

bool TestLicenseServer::Start() {
  if (license_server_process_ != base::kNullProcessHandle)
    return true;

  if (!server_config_->IsPlatformSupported()) {
    VLOG(0) << "License server is not supported on current platform.";
    return false;
  }

  CommandLine command_line(CommandLine::NO_PROGRAM);
  if (!server_config_->GetServerCommandLine(&command_line)) {
    VLOG(0) << "Could not get server command line to launch.";
    return false;
  }

  VLOG(0) << "Starting test license server " <<
      command_line.GetCommandLineString();
  if (!base::LaunchProcess(command_line, base::LaunchOptions(),
                           &license_server_process_)) {
    VLOG(0) << "Failed to start test license server!";
    return false;
  }
  DCHECK_NE(license_server_process_, base::kNullProcessHandle);
  return true;
}

bool TestLicenseServer::Stop() {
  if (license_server_process_ == base::kNullProcessHandle)
    return true;
  VLOG(0) << "Killing license server.";
  bool kill_succeeded = base::KillProcess(license_server_process_, 1, true);

  if (kill_succeeded) {
    base::CloseProcessHandle(license_server_process_);
    license_server_process_ = base::kNullProcessHandle;
  } else {
    VLOG(1) << "Kill failed?!";
  }
  return kill_succeeded;
}

std::string TestLicenseServer::GetServerURL() {
  return server_config_->GetServerURL();
}

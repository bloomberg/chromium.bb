// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/wv_test_license_server_config.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"

const uint16 kDefaultPort = 8888;

// Widevine license server configuration files.
const base::FilePath::CharType kKeysFileName[] =
    FILE_PATH_LITERAL("keys.dat");
const base::FilePath::CharType kPoliciesFileName[] =
    FILE_PATH_LITERAL("policies.dat");
const base::FilePath::CharType kProfilesFileName[] =
    FILE_PATH_LITERAL("profiles.dat");

// License server root path relative to source dir.
const base::FilePath::CharType kLicenseServerRootPath[] =
    FILE_PATH_LITERAL("chrome/test/media/license_server/widevine");

// License server configuration files directory name relative to root.
const base::FilePath::CharType kLicenseServerConfigDirName[] =
    FILE_PATH_LITERAL("config");

// Platform-specific license server binary path relative to root.
const base::FilePath::CharType kLicenseServerBinPath[] =
#if defined(OS_LINUX)
    FILE_PATH_LITERAL("linux/license_server");
#else
    FILE_PATH_LITERAL("unsupported_platform");
#endif  // defined(OS_LINUX)

WVTestLicenseServerConfig::WVTestLicenseServerConfig() {
}

WVTestLicenseServerConfig::~WVTestLicenseServerConfig() {
}

void WVTestLicenseServerConfig::AppendCommandLineArgs(
    CommandLine* command_line) {
  base::FilePath source_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &source_root);
  base::FilePath config_path =
      source_root.Append(kLicenseServerRootPath)
                 .Append(kLicenseServerConfigDirName);

  if (!base::PathExists(config_path.Append(kKeysFileName)) ||
      !base::PathExists(config_path.Append(kPoliciesFileName)) ||
      !base::PathExists(config_path.Append(kProfilesFileName))) {
    CHECK(false) << "Missing license server configuration files.";
    return;
  }
  command_line->AppendSwitchPath("key_file", config_path.Append(kKeysFileName));
  command_line->AppendSwitchPath("policies_file",
      config_path.Append(kPoliciesFileName));
  command_line->AppendSwitchPath("profiles_file",
      config_path.Append(kProfilesFileName));
  command_line->AppendSwitchASCII("port",
      base::StringPrintf("%u", kDefaultPort));
  command_line->AppendSwitch("allow_unknown_devices");
}

bool WVTestLicenseServerConfig::IsPlatformSupported() {
  // TODO(shadi): Enable on Linux once crbug.com/339983 is resolved.
  return false;
}

std::string WVTestLicenseServerConfig::GetServerURL() {
  return base::StringPrintf("http://localhost:%u/license_server", kDefaultPort);
}

void WVTestLicenseServerConfig::GetLicenseServerPath(base::FilePath *path) {
  base::FilePath source_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &source_root);
  *path = source_root.Append(kLicenseServerRootPath)
                     .Append(kLicenseServerBinPath);
}

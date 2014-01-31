// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WV_TEST_LICENSE_SERVER_CONFIG_H_
#define CHROME_BROWSER_MEDIA_WV_TEST_LICENSE_SERVER_CONFIG_H_

#include "chrome/browser/media/test_license_server_config.h"

// License configuration to run the Widevine test license server.
class WVTestLicenseServerConfig : public TestLicenseServerConfig {
 public:
  WVTestLicenseServerConfig();
  virtual ~WVTestLicenseServerConfig();

  virtual std::string GetServerURL() OVERRIDE;

  // Retrieves the path for the license server binaries.
  // WV license server files are located under:
  // chrome/test/license_server/<platform>/
  virtual void GetLicenseServerPath(base::FilePath* path) OVERRIDE;

  // Appends necessary Widevine license server command line arguments:
  // port number, keys file path, policies file path, and profiles file path.
  virtual void AppendCommandLineArgs(CommandLine* command_line) OVERRIDE;

  virtual bool IsPlatformSupported() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WVTestLicenseServerConfig);
};

#endif  // CHROME_BROWSER_MEDIA_WV_TEST_LICENSE_SERVER_CONFIG_H_

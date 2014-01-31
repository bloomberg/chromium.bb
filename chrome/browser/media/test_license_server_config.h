// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_CONFIG_H_
#define CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_CONFIG_H_

#include <string>
#include "base/basictypes.h"

class CommandLine;

namespace base {
  class FilePath;
}

// Class used to start a test license server.
class TestLicenseServerConfig {
 public:
  TestLicenseServerConfig() {}
  virtual ~TestLicenseServerConfig() {}

  // Returns a string containing the URL and port the server is listening to.
  // This URL is directly used by the Web app to request a license, example:
  // http://localhost:8888/license_server
  virtual std::string GetServerURL() = 0;

  // Retrieves the path for the license server binaries.
  virtual void GetLicenseServerPath(base::FilePath* path) = 0;

  // Appends any necessary server command line arguments.
  virtual void AppendCommandLineArgs(CommandLine* command_line) {}

  // Returns true if the server is supported on current platform.
  virtual bool IsPlatformSupported() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLicenseServerConfig);
};

#endif  // CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_CONFIG_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_H_
#define CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_H_

#include <string>
#include "base/memory/scoped_ptr.h"
#include "base/process/process_handle.h"

class TestLicenseServerConfig;

// Class used to start a test license server.
class TestLicenseServer {
 public:
  explicit TestLicenseServer(scoped_ptr<TestLicenseServerConfig> server_config);
  ~TestLicenseServer();

  // Returns true if the server started successfully. False otherwise.
  bool Start();
  // Returns true if the server was stopped successfully. False otherwise.
  bool Stop();
  // Returns a string containing the URL and port the server is listening to.
  std::string GetServerURL();

 private:
  // License server configuration class used to obtain server paths, etc.
  scoped_ptr<TestLicenseServerConfig> server_config_;
  // Process handle for the license server.
  base::ProcessHandle license_server_process_;

  DISALLOW_COPY_AND_ASSIGN(TestLicenseServer);
};

#endif  // CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_H_

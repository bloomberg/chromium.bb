// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_TEST_LOCAL_POLICY_TEST_SERVER_H_
#define CHROME_BROWSER_POLICY_TEST_LOCAL_POLICY_TEST_SERVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "net/test/local_test_server.h"

namespace policy {

// Runs a python implementation of the cloud policy server on the local machine.
class LocalPolicyTestServer : public net::LocalTestServer {
 public:
  // Initializes a test server configured by the configuration file
  // |config_file|.
  explicit LocalPolicyTestServer(const base::FilePath& config_file);

  // Initializes the test server with the configuration read from
  // chrome/test/data/policy/policy_|test_name|.json.
  explicit LocalPolicyTestServer(const std::string& test_name);

  virtual ~LocalPolicyTestServer();

  // Gets the service URL.
  GURL GetServiceURL() const;

  // net::LocalTestServer:
  virtual bool SetPythonPath() const OVERRIDE;
  virtual bool GetTestServerPath(
      base::FilePath* testserver_path) const OVERRIDE;
  virtual bool GenerateAdditionalArguments(
      base::DictionaryValue* arguments) const OVERRIDE;

 private:
  base::FilePath config_file_;

  DISALLOW_COPY_AND_ASSIGN(LocalPolicyTestServer);
};

}  // namespace

#endif  // CHROME_BROWSER_POLICY_TEST_LOCAL_POLICY_TEST_SERVER_H_

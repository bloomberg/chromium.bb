// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/test/local_policy_test_server.h"

#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "net/test/base_test_server.h"
#include "net/test/python_utils.h"

namespace policy {

LocalPolicyTestServer::LocalPolicyTestServer(const base::FilePath& config_file)
    : net::LocalTestServer(net::BaseTestServer::TYPE_HTTP,
                           net::BaseTestServer::kLocalhost,
                           base::FilePath()),
      config_file_(config_file) {}

LocalPolicyTestServer::LocalPolicyTestServer(const std::string& test_name)
    : net::LocalTestServer(net::BaseTestServer::TYPE_HTTP,
                           net::BaseTestServer::kLocalhost,
                           base::FilePath()) {
  // Read configuration from a file in chrome/test/data/policy.
  base::FilePath source_root;
  CHECK(PathService::Get(chrome::DIR_TEST_DATA, &source_root));
  config_file_ = source_root
      .AppendASCII("policy")
      .AppendASCII(base::StringPrintf("policy_%s.json", test_name.c_str()));
}

LocalPolicyTestServer::~LocalPolicyTestServer() {}

GURL LocalPolicyTestServer::GetServiceURL() const {
  return GetURL("device_management");
}

bool LocalPolicyTestServer::SetPythonPath() const {
  if (!net::LocalTestServer::SetPythonPath())
    return false;

  // Add the net/tools/testserver directory to the path.
  base::FilePath net_testserver_path;
  if (!LocalTestServer::GetTestServerPath(&net_testserver_path)) {
    LOG(ERROR) << "Failed to get net testserver path.";
    return false;
  }
  AppendToPythonPath(net_testserver_path.DirName());

  // Add the generated python protocol buffer bindings.
  base::FilePath pyproto_dir;
  if (!GetPyProtoPath(&pyproto_dir)) {
    LOG(ERROR) << "Cannot find pyproto dir for generated code.";
    return false;
  }

  AppendToPythonPath(pyproto_dir
                     .AppendASCII("chrome")
                     .AppendASCII("browser")
                     .AppendASCII("policy")
                     .AppendASCII("proto"));
  return true;
}

bool LocalPolicyTestServer::GetTestServerPath(
    base::FilePath* testserver_path) const {
  base::FilePath source_root;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &source_root)) {
    LOG(ERROR) << "Failed to get DIR_SOURCE_ROOT";
    return false;
  }
  *testserver_path = source_root
      .AppendASCII("chrome")
      .AppendASCII("browser")
      .AppendASCII("policy")
      .AppendASCII("test")
      .AppendASCII("policy_testserver.py");
  return true;
}

bool LocalPolicyTestServer::GenerateAdditionalArguments(
    base::DictionaryValue* arguments) const {
  if (!net::LocalTestServer::GenerateAdditionalArguments(arguments))
    return false;

  arguments->SetString("config-file", config_file_.AsUTF8Unsafe());
  return true;
}

}  // namespace policy;

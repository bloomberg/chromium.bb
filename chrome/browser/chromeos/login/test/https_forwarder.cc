// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/https_forwarder.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "net/test/python_utils.h"

namespace chromeos {

HTTPSForwarder::HTTPSForwarder(const GURL& forward_target)
    : net::LocalTestServer(net::LocalTestServer::TYPE_HTTPS,
                           net::LocalTestServer::kLocalhost,
                           base::FilePath()),
      forward_target_(forward_target) {
}

HTTPSForwarder::~HTTPSForwarder() {
}

bool HTTPSForwarder::SetPythonPath() const {
  if (!net::LocalTestServer::SetPythonPath())
    return false;

  base::FilePath net_testserver_path;
  if (!LocalTestServer::GetTestServerPath(&net_testserver_path))
    return false;
  AppendToPythonPath(net_testserver_path.DirName());

  return true;
}

bool HTTPSForwarder::GetTestServerPath(base::FilePath* testserver_path) const {
  base::FilePath source_root_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir))
    return false;

  *testserver_path = source_root_dir.Append("chrome")
                                    .Append("browser")
                                    .Append("chromeos")
                                    .Append("login")
                                    .Append("test")
                                    .Append("https_forwarder.py");
  return true;
}

bool HTTPSForwarder::GenerateAdditionalArguments(
    base::DictionaryValue* arguments) const {
  base::FilePath source_root_dir;
  if (!net::LocalTestServer::GenerateAdditionalArguments(arguments) ||
      !PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir))
    return false;

  arguments->SetString("forward-target", forward_target_.spec());
  return true;
}

}  // namespace chromeos

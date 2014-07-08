// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_base.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
namespace test {

ShellTestBase::ShellTestBase() {
}

ShellTestBase::~ShellTestBase() {
}

ScopedMessagePipeHandle ShellTestBase::ConnectToService(
    const GURL& application_url,
    const std::string& service_name) {
  base::FilePath service_dir;
  CHECK(PathService::Get(base::DIR_MODULE, &service_dir));
  shell_context_.mojo_url_resolver()->set_origin(
      net::FilePathToFileURL(service_dir).spec());

  return shell_context_.service_manager()->ConnectToServiceByName(
      application_url, service_name).Pass();
}

}  // namespace test
}  // namespace shell
}  // namespace mojo

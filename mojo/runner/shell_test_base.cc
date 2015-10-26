// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/shell_test_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"

namespace mojo {
namespace runner {
namespace test {

namespace {

void QuitIfRunning() {
  if (base::MessageLoop::current() &&
      base::MessageLoop::current()->is_running()) {
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

}  // namespace

ShellTestBase::ShellTestBase()
    : shell_context_(GetTestAppFilePath(), nullptr) {}

ShellTestBase::~ShellTestBase() {
}

void ShellTestBase::SetUp() {
  CHECK(shell_context_.Init());
}

void ShellTestBase::TearDown() {
  shell_context_.Shutdown();
}

ScopedMessagePipeHandle ShellTestBase::ConnectToService(
    const GURL& application_url,
    const std::string& service_name) {
  ServiceProviderPtr services;

  scoped_ptr<shell::ConnectToApplicationParams> params(
      new shell::ConnectToApplicationParams);
  params->SetTarget(shell::Identity(application_url, std::string(),
                                    shell::GetPermissiveCapabilityFilter()));
  params->set_services(GetProxy(&services));
  params->set_on_application_end(base::Bind(&QuitIfRunning));
  shell_context_.application_manager()->ConnectToApplication(params.Pass());
  MessagePipe pipe;
  services->ConnectToService(service_name, pipe.handle1.Pass());
  return pipe.handle0.Pass();
}

#if !defined(OS_ANDROID)
base::FilePath ShellTestBase::GetTestAppFilePath() const {
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  return shell_dir;
}
#endif

}  // namespace test
}  // namespace runner
}  // namespace mojo

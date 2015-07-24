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

ShellTestBase::ShellTestBase() {
}

ShellTestBase::~ShellTestBase() {
}

void ShellTestBase::SetUp() {
  CHECK(shell_context_.Init());
  SetUpTestApplications();
}

void ShellTestBase::TearDown() {
  shell_context_.Shutdown();
}

ScopedMessagePipeHandle ShellTestBase::ConnectToService(
    const GURL& application_url,
    const std::string& service_name) {
  ServiceProviderPtr services;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(application_url.spec());
  shell_context_.application_manager()->ConnectToApplication(
      nullptr, request.Pass(), std::string(), GURL(), GetProxy(&services),
      nullptr, nullptr, base::Bind(&QuitIfRunning));
  MessagePipe pipe;
  services->ConnectToService(service_name, pipe.handle1.Pass());
  return pipe.handle0.Pass();
}

#if !defined(OS_ANDROID)
void ShellTestBase::SetUpTestApplications() {
  // Set the URLResolver origin to be the same as the base file path for
  // local files. This is primarily for test convenience, so that references
  // to unknown mojo: URLs that do not have specific local file or custom
  // mappings registered on the URL resolver are treated as shared libraries.
  base::FilePath service_dir;
  CHECK(PathService::Get(base::DIR_MODULE, &service_dir));
  shell_context_.url_resolver()->SetMojoBaseURL(
      util::FilePathToFileURL(service_dir));
}
#endif

}  // namespace test
}  // namespace runner
}  // namespace mojo

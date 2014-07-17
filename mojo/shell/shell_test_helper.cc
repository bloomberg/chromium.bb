// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_helper.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "mojo/shell/init.h"
#include "net/base/filename_util.h"

namespace mojo {
namespace shell {

ShellTestHelper::ShellTestHelper() {
  base::CommandLine::Init(0, NULL);
  mojo::shell::InitializeLogging();
}

ShellTestHelper::~ShellTestHelper() {
}

void ShellTestHelper::Init() {
  context_.reset(new Context);
  test_api_.reset(new ServiceManager::TestAPI(context_->service_manager()));
  base::FilePath service_dir;
  CHECK(PathService::Get(base::DIR_MODULE, &service_dir));
  context_->mojo_url_resolver()->set_origin(
      net::FilePathToFileURL(service_dir).spec());
}

void ShellTestHelper::SetLoaderForURL(scoped_ptr<ServiceLoader> loader,
                                      const GURL& url) {
  context_->service_manager()->SetLoaderForURL(loader.Pass(), url);
}

}  // namespace shell
}  // namespace mojo

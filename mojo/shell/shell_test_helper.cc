// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_helper.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "mojo/shell/init.h"

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
}

void ShellTestHelper::SetLoaderForURL(scoped_ptr<ServiceLoader> loader,
                                      const GURL& url) {
  context_->service_manager()->SetLoaderForURL(loader.Pass(), url);
}

}  // namespace shell
}  // namespace mojo

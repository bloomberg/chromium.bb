// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/mojo_runner_delegate.h"

#include "base/path_service.h"
#include "gin/modules/console.h"
#include "gin/modules/module_registry.h"
#include "gin/try_catch.h"
#include "mojo/apps/js/bootstrap.h"
#include "mojo/public/bindings/js/core.h"
#include "mojo/public/bindings/js/support.h"

namespace mojo {
namespace apps {

namespace {

// TODO(abarth): Rather than loading these modules from the file system, we
// should load them from the network via Mojo IPC.
std::vector<base::FilePath> GetModuleSearchPaths() {
  std::vector<base::FilePath> search_paths(2);
  PathService::Get(base::DIR_SOURCE_ROOT, &search_paths[0]);
  PathService::Get(base::DIR_EXE, &search_paths[1]);
  search_paths[1] = search_paths[1].AppendASCII("gen");
  return search_paths;
}

}  // namespace

MojoRunnerDelegate::MojoRunnerDelegate()
    : ModuleRunnerDelegate(GetModuleSearchPaths()) {
  AddBuiltinModule(Bootstrap::kModuleName, Bootstrap::GetTemplate);
  AddBuiltinModule(gin::Console::kModuleName, gin::Console::GetTemplate);
  AddBuiltinModule(js::Core::kModuleName, js::Core::GetTemplate);
  AddBuiltinModule(js::Support::kModuleName, js::Support::GetTemplate);
}

MojoRunnerDelegate::~MojoRunnerDelegate() {
}

void MojoRunnerDelegate::UnhandledException(gin::Runner* runner,
                                            gin::TryCatch& try_catch) {
  gin::ModuleRunnerDelegate::UnhandledException(runner, try_catch);
  LOG(ERROR) << try_catch.GetStackTrace();
}

}  // namespace apps
}  // namespace mojo

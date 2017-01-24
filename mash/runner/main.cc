// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "services/service_manager/runner/init.h"
#include "services/service_manager/standalone/context.h"
#include "services/service_manager/switches.h"

namespace {

const base::FilePath::CharType kMashCatalogFilename[] =
    FILE_PATH_LITERAL("mash_catalog.json");

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  base::AtExitManager at_exit;
  service_manager::InitializeLogging();
  service_manager::WaitForDebuggerIfNecessary();

#if !defined(OFFICIAL_BUILD) && defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif

#if !defined(OFFICIAL_BUILD)
  base::debug::EnableInProcessStackDumping();
#endif
  base::PlatformThread::SetName("service_manager");

  std::string catalog_contents;
  base::FilePath exe_path;
  base::PathService::Get(base::DIR_EXE, &exe_path);
  base::FilePath catalog_path = exe_path.Append(kMashCatalogFilename);
  DCHECK(base::ReadFileToString(catalog_path, &catalog_contents));
  std::unique_ptr<base::Value> manifest_value =
      base::JSONReader::Read(catalog_contents);
  DCHECK(manifest_value);

  auto params = base::MakeUnique<service_manager::Context::InitParams>();
  params->static_catalog = std::move(manifest_value);

#if defined(OS_WIN) && defined(COMPONENT_BUILD)
  // In Windows component builds, ensure that loaded service binaries always
  // search this exe's dir for DLLs.
  SetDllDirectory(exe_path.value().c_str());
#endif

  // We want the Context to outlive the MessageLoop so that pipes are all
  // gracefully closed / error-out before we try to shut the Context down.
  service_manager::Context service_manager_context;
  {
    base::MessageLoop message_loop;
    base::i18n::InitializeICU();
    service_manager_context.Init(std::move(params));

    message_loop.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&service_manager::Context::RunCommandLineApplication,
                   base::Unretained(&service_manager_context)));

    base::RunLoop().Run();

    // Must be called before |message_loop| is destroyed.
    service_manager_context.Shutdown();
  }

  return 0;
}

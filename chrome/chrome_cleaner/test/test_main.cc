// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/chrome_cleaner/os/rebooter.h"
#include "chrome/chrome_cleaner/os/secure_dll_loading.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace {

bool IsSandboxedProcess() {
  static const bool is_sandboxed_process =
      (sandbox::SandboxFactory::GetTargetServices() != nullptr);
  return is_sandboxed_process;
}

}  // namespace

int main(int argc, char** argv) {
  // This must be executed as soon as possible to reduce the number of dlls that
  // the code might try to load before we can lock things down.
  //
  // We enable secure DLL loading in the test suite to be sure that it doesn't
  // affect the behaviour of functionality that's tested.
  chrome_cleaner::EnableSecureDllLoading();

  base::TestSuite test_suite(argc, argv);

  if (!chrome_cleaner::SetupTestConfigs())
    return 1;

  if (!chrome_cleaner::CheckTestPrivileges())
    return 1;

  // Make sure tests will not end up in an infinite reboot loop.
  if (chrome_cleaner::Rebooter::IsPostReboot())
    return 0;

  // ScopedCOMInitializer keeps COM initialized in a specific scope. We don't
  // want to initialize it for sandboxed processes, so manage its lifetime with
  // a unique_ptr, which will call ScopedCOMInitializer's destructor when it
  // goes out of scope below.
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer;

  if (!IsSandboxedProcess()) {
    scoped_com_initializer = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
    bool success = chrome_cleaner::InitializeCOMSecurity();
    DCHECK(success) << "InitializeCOMSecurity() failed.";

    success = chrome_cleaner::TaskScheduler::Initialize();
    DCHECK(success) << "TaskScheduler::Initialize() failed.";
  }

  // Some tests will fail if two tests try to launch test_process.exe
  // simultaneously, so run the tests serially. This will still shard them and
  // distribute the shards to different swarming bots, but tests will run
  // serially on each bot.
  const int result = base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));

  if (!IsSandboxedProcess())
    chrome_cleaner::TaskScheduler::Terminate();

  return result;
}

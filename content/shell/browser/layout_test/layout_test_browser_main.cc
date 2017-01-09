// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_browser_main.h"

#include <iostream>
#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/browser/layout_test/test_info_extractor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/layout_test/blink_test_helpers.h"
#include "net/base/filename_util.h"

#if defined(OS_ANDROID)
#include "base/run_loop.h"
#include "content/shell/browser/layout_test/scoped_android_configuration.h"
#endif

namespace {

bool RunOneTest(
    const content::TestInfo& test_info,
    bool* ran_at_least_once,
    const std::unique_ptr<content::BrowserMainRunner>& main_runner) {
  if (!content::BlinkTestController::Get()->PrepareForLayoutTest(
          test_info.url, test_info.current_working_directory,
          test_info.enable_pixel_dumping, test_info.expected_pixel_hash)) {
    return false;
  }

  *ran_at_least_once = true;
#if defined(OS_ANDROID)
  // The message loop on Android is provided by the system, and does not
  // offer a blocking Run() method. For layout tests, use a nested loop
  // together with a base::RunLoop so it can block until a QuitClosure.
  base::RunLoop run_loop;
  run_loop.Run();
#else
  main_runner->Run();
#endif

  if (!content::BlinkTestController::Get()->ResetAfterLayoutTest())
    return false;

#if defined(OS_ANDROID)
  // There will be left-over tasks in the queue for Android because the
  // main window is being destroyed. Run them before starting the next test.
  base::RunLoop().RunUntilIdle();
#endif
  return true;
}

int RunTests(const std::unique_ptr<content::BrowserMainRunner>& main_runner) {
  content::BlinkTestController test_controller;
  {
    // We're outside of the message loop here, and this is a test.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath temp_path;
    base::GetTempDir(&temp_path);
    test_controller.SetTempPath(temp_path);
  }

  std::cout << "#READY\n";
  std::cout.flush();

  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  content::TestInfoExtractor test_extractor(args);
  bool ran_at_least_once = false;
  std::unique_ptr<content::TestInfo> test_info;
  while ((test_info = test_extractor.GetNextTest())) {
    if (!RunOneTest(*test_info, &ran_at_least_once, main_runner))
      break;
  }
  if (!ran_at_least_once) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    main_runner->Run();
  }

#if defined(OS_ANDROID)
  // We need to execute 'main_runner->Shutdown()' before the test_controller
  // destructs when running on Android, and after it destructs when running
  // anywhere else.
  main_runner->Shutdown();
#endif

  return 0;
}

}  // namespace

// Main routine for running as the Browser process.
int LayoutTestBrowserMain(
    const content::MainFunctionParams& parameters,
    const std::unique_ptr<content::BrowserMainRunner>& main_runner) {
  base::ScopedTempDir browser_context_path_for_layout_tests;

  CHECK(browser_context_path_for_layout_tests.CreateUniqueTempDir());
  CHECK(
      !browser_context_path_for_layout_tests.GetPath().MaybeAsASCII().empty());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kContentShellDataPath,
      browser_context_path_for_layout_tests.GetPath().MaybeAsASCII());

#if defined(OS_ANDROID)
  content::ScopedAndroidConfiguration android_configuration;
#endif

  int exit_code = main_runner->Initialize(parameters);
  DCHECK_LT(exit_code, 0)
      << "BrowserMainRunner::Initialize failed in LayoutTestBrowserMain";

  if (exit_code >= 0)
    return exit_code;

#if defined(OS_ANDROID)
  android_configuration.RedirectStreams();
#endif

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCheckLayoutTestSysDeps)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    main_runner->Run();
    content::Shell::CloseAllWindows();
    main_runner->Shutdown();
    return 0;
  }

  exit_code = RunTests(main_runner);
  base::RunLoop().RunUntilIdle();

  content::Shell::CloseAllWindows();
#if !defined(OS_ANDROID)
  main_runner->Shutdown();
#endif

  return exit_code;
}

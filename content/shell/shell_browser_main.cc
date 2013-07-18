// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_browser_main.h"

#include <iostream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/webkit_test_helpers.h"
#include "content/shell/shell.h"
#include "content/shell/webkit_test_controller.h"
#include "net/base/net_util.h"
#include "webkit/support/webkit_support.h"

#if defined(OS_ANDROID)
#include "base/run_loop.h"
#include "content/shell/shell_layout_tests_android.h"
#endif

namespace {

GURL GetURLForLayoutTest(const std::string& test_name,
                         base::FilePath* current_working_directory,
                         bool* enable_pixel_dumping,
                         std::string* expected_pixel_hash) {
  // A test name is formated like file:///path/to/test'--pixel-test'pixelhash
  std::string path_or_url = test_name;
  std::string pixel_switch;
  std::string pixel_hash;
  std::string::size_type separator_position = path_or_url.find('\'');
  if (separator_position != std::string::npos) {
    pixel_switch = path_or_url.substr(separator_position + 1);
    path_or_url.erase(separator_position);
  }
  separator_position = pixel_switch.find('\'');
  if (separator_position != std::string::npos) {
    pixel_hash = pixel_switch.substr(separator_position + 1);
    pixel_switch.erase(separator_position);
  }
  if (enable_pixel_dumping) {
    *enable_pixel_dumping =
        (pixel_switch == "--pixel-test" || pixel_switch == "-p");
  }
  if (expected_pixel_hash)
    *expected_pixel_hash = pixel_hash;

  GURL test_url;
#if defined(OS_ANDROID)
  if (content::GetTestUrlForAndroid(path_or_url, &test_url))
    return test_url;
#endif

  test_url = GURL(path_or_url);
  if (!(test_url.is_valid() && test_url.has_scheme())) {
    // We're outside of the message loop here, and this is a test.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
#if defined(OS_WIN)
    std::wstring wide_path_or_url =
        base::SysNativeMBToWide(path_or_url);
    base::FilePath local_file(wide_path_or_url);
#else
    base::FilePath local_file(path_or_url);
#endif
    if (!base::PathExists(local_file)) {
      local_file = content::GetWebKitRootDirFilePath()
          .Append(FILE_PATH_LITERAL("LayoutTests")).Append(local_file);
    }
    test_url = net::FilePathToFileURL(base::MakeAbsoluteFilePath(local_file));
  }
  base::FilePath local_path;
  if (current_working_directory) {
    // We're outside of the message loop here, and this is a test.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (net::FileURLToFilePath(test_url, &local_path))
      *current_working_directory = local_path.DirName();
    else
      file_util::GetCurrentDirectory(current_working_directory);
  }
  return test_url;
}

bool GetNextTest(const CommandLine::StringVector& args,
                 size_t* position,
                 std::string* test) {
  if (*position >= args.size())
    return false;
  if (args[*position] == FILE_PATH_LITERAL("-"))
    return !!std::getline(std::cin, *test, '\n');
#if defined(OS_WIN)
  *test = WideToUTF8(args[(*position)++]);
#else
  *test = args[(*position)++];
#endif
  return true;
}

}  // namespace

// Main routine for running as the Browser process.
int ShellBrowserMain(
    const content::MainFunctionParams& parameters,
    const scoped_ptr<content::BrowserMainRunner>& main_runner) {
  bool layout_test_mode =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree);
  base::ScopedTempDir browser_context_path_for_layout_tests;

  if (layout_test_mode) {
    CHECK(browser_context_path_for_layout_tests.CreateUniqueTempDir());
    CHECK(!browser_context_path_for_layout_tests.path().MaybeAsASCII().empty());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kContentShellDataPath,
        browser_context_path_for_layout_tests.path().MaybeAsASCII());

#if defined(OS_ANDROID)
    content::EnsureInitializeForAndroidLayoutTests();
#endif
  }

  int exit_code = main_runner->Initialize(parameters);
  DCHECK_LT(exit_code, 0)
      << "BrowserMainRunner::Initialize failed in ShellBrowserMain";

  if (exit_code >= 0)
    return exit_code;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kCheckLayoutTestSysDeps)) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    main_runner->Run();
    content::Shell::CloseAllWindows();
    main_runner->Shutdown();
    return 0;
  }

  if (layout_test_mode) {
    content::WebKitTestController test_controller;
    {
      // We're outside of the message loop here, and this is a test.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      base::FilePath temp_path;
      file_util::GetTempDir(&temp_path);
      test_controller.SetTempPath(temp_path);
    }
    std::string test_string;
    CommandLine::StringVector args =
        CommandLine::ForCurrentProcess()->GetArgs();
    size_t command_line_position = 0;
    bool ran_at_least_once = false;

#if defined(OS_ANDROID)
    std::cout << "#READY\n";
    std::cout.flush();
#endif

    while (GetNextTest(args, &command_line_position, &test_string)) {
      if (test_string.empty())
        continue;
      if (test_string == "QUIT")
        break;

      bool enable_pixel_dumps;
      std::string pixel_hash;
      base::FilePath cwd;
      GURL test_url = GetURLForLayoutTest(
          test_string, &cwd, &enable_pixel_dumps, &pixel_hash);
      if (!content::WebKitTestController::Get()->PrepareForLayoutTest(
              test_url, cwd, enable_pixel_dumps, pixel_hash)) {
        break;
      }

      ran_at_least_once = true;
#if defined(OS_ANDROID)
      // The message loop on Android is provided by the system, and does not
      // offer a blocking Run() method. For layout tests, use a nested loop
      // together with a base::RunLoop so it can block until a QuitClosure.
      base::RunLoop run_loop;
      run_loop.Run();
#else
      main_runner->Run();
#endif

      if (!content::WebKitTestController::Get()->ResetAfterLayoutTest())
        break;
    }
    if (!ran_at_least_once) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
      main_runner->Run();
    }
    exit_code = 0;
  }

#if !defined(OS_ANDROID)
  if (!layout_test_mode)
    exit_code = main_runner->Run();

  main_runner->Shutdown();
#endif

  return exit_code;
}

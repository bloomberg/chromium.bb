// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/nacl_host/nacl_browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ppapi/ppapi_test.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"

class NaClGdbDebugStubTest : public PPAPINaClNewlibTest {
 public:
  NaClGdbDebugStubTest() {
  }

  void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  void StartTestScript(base::ProcessHandle* test_process,
                       std::string test_name, int debug_stub_port);

};

void NaClGdbDebugStubTest::SetUpCommandLine(CommandLine* command_line) {
  PPAPINaClNewlibTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableNaClDebug);
}

void NaClGdbDebugStubTest::StartTestScript(base::ProcessHandle* test_process,
                                           std::string test_name,
                                           int debug_stub_port) {
  // We call python script to reuse GDB RSP protocol implementation.
  CommandLine cmd(FilePath(FILE_PATH_LITERAL("python")));
  FilePath script;
  PathService::Get(base::DIR_SOURCE_ROOT, &script);
  script = script.AppendASCII(
      "chrome/browser/nacl_host/test/debug_stub_browser_tests.py");
  cmd.AppendArgPath(script);
  cmd.AppendArg(base::IntToString(debug_stub_port));
  cmd.AppendArg(test_name);
  LOG(INFO) << cmd.GetCommandLineString();
  base::LaunchProcess(cmd, base::LaunchOptions(), test_process);
}

// NaCl tests are disabled under ASAN because of qualification test.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_Empty DISABLED_Empty
#else
#define MAYBE_Empty Empty
#endif

IN_PROC_BROWSER_TEST_F(NaClGdbDebugStubTest, MAYBE_Empty) {
  base::ProcessHandle test_script;
  NaClBrowser::GetInstance()->SetGdbDebugStubPortListener(
      base::Bind(&NaClGdbDebugStubTest::StartTestScript,
                 base::Unretained(this), &test_script, "continue"));
  RunTestViaHTTP("Empty");
  NaClBrowser::GetInstance()->ClearGdbDebugStubPortListener();
  int exit_code;
  base::WaitForExitCode(test_script, &exit_code);
  EXPECT_EQ(0, exit_code);
}

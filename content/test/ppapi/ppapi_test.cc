// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/ppapi/ppapi_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "ppapi/shared_impl/test_harness_utils.h"

namespace content {

PPAPITestMessageHandler::PPAPITestMessageHandler() {
}

TestMessageHandler::MessageResponse PPAPITestMessageHandler::HandleMessage(
    const std::string& json) {
  std::string trimmed;
  base::TrimString(json, "\"", &trimmed);
  if (trimmed == "...")
    return CONTINUE;
  message_ = trimmed;
  return DONE;
}

void PPAPITestMessageHandler::Reset() {
  TestMessageHandler::Reset();
  message_.clear();
}

PPAPITestBase::PPAPITestBase() { }

void PPAPITestBase::SetUpCommandLine(base::CommandLine* command_line) {
  // The test sends us the result via a cookie.
  command_line->AppendSwitch(switches::kEnableFileCookies);

  // Some stuff is hung off of the testing interface which is not enabled
  // by default.
  command_line->AppendSwitch(switches::kEnablePepperTesting);

  // Smooth scrolling confuses the scrollbar test.
  command_line->AppendSwitch(switches::kDisableSmoothScrolling);

  // Allow manual garbage collection.
  command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose_gc");
}

GURL PPAPITestBase::GetTestFileUrl(const std::string& test_case) {
  base::FilePath test_path;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_path));
  test_path = test_path.Append(FILE_PATH_LITERAL("ppapi"));
  test_path = test_path.Append(FILE_PATH_LITERAL("tests"));
  test_path = test_path.Append(FILE_PATH_LITERAL("test_case.html"));

  // Sanity check the file name.
  EXPECT_TRUE(base::PathExists(test_path));
  GURL test_url = net::FilePathToFileURL(test_path);

  GURL::Replacements replacements;
  std::string query = BuildQuery(std::string(), test_case);
  replacements.SetQuery(query.c_str(), url::Component(0, query.size()));
  return test_url.ReplaceComponents(replacements);
}

void PPAPITestBase::RunTest(const std::string& test_case) {
  GURL url = GetTestFileUrl(test_case);
  RunTestURL(url);
}

void PPAPITestBase::RunTestAndReload(const std::string& test_case) {
  GURL url = GetTestFileUrl(test_case);
  RunTestURL(url);
  // If that passed, we simply run the test again, which navigates again.
  RunTestURL(url);
}

void PPAPITestBase::RunTestURL(const GURL& test_url) {
  // See comment above TestingInstance in ppapi/test/testing_instance.h.
  // Basically it sends messages using the DOM automation controller. The
  // value of "..." means it's still working and we should continue to wait,
  // any other value indicates completion (in this case it will start with
  // "PASS" or "FAIL"). This keeps us from timing out on waits for long tests.
  PPAPITestMessageHandler handler;
  JavascriptTestObserver observer(shell()->web_contents(), &handler);
  shell()->LoadURL(test_url);

  ASSERT_TRUE(observer.Run()) << handler.error_message();
  EXPECT_STREQ("PASS", handler.message().c_str());
}

PPAPITest::PPAPITest() : in_process_(true) {
}

void PPAPITest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);

  // Append the switch to register the pepper plugin.
  // library name = <out dir>/<test_name>.<library_extension>
  // MIME type = application/x-ppapi-<test_name>
  base::FilePath plugin_dir;
  EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));

  base::FilePath plugin_lib = plugin_dir.Append(ppapi::GetTestLibraryName());
  EXPECT_TRUE(base::PathExists(plugin_lib));
  base::FilePath::StringType pepper_plugin = plugin_lib.value();
  pepper_plugin.append(FILE_PATH_LITERAL(";application/x-ppapi-tests"));
  command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                   pepper_plugin);

  if (in_process_)
    command_line->AppendSwitch(switches::kPpapiInProcess);
}

std::string PPAPITest::BuildQuery(const std::string& base,
                                  const std::string& test_case){
  return base::StringPrintf("%stestcase=%s", base.c_str(), test_case.c_str());
}

OutOfProcessPPAPITest::OutOfProcessPPAPITest() {
  in_process_ = false;
}

}  // namespace content

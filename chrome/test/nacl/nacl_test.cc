// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/nacl_test.h"

#include "base/file_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";

static const FilePath::CharType kBaseUrl[] =
    FILE_PATH_LITERAL("http://localhost:5103/");

static const FilePath::CharType kSrpcHwHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_hw.html");
static const FilePath::CharType kSrpcHwNexeFileName[] =
    FILE_PATH_LITERAL("srpc_hw.nexe");

static const FilePath::CharType kServerHtmlFileName[] =
    FILE_PATH_LITERAL("server_test.html");


NaClTest::NaClTest()
    : UITest() {
  launch_arguments_.AppendSwitch(switches::kInternalNaCl);
}

NaClTest::~NaClTest() {}

FilePath NaClTest::GetTestRootDir() {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("native_client");
  path = path.AppendASCII("tests");
  return path;
}

// Static
GURL NaClTest::GetTestUrl(const FilePath& filename) {
  FilePath path(kBaseUrl);
  path = path.Append(filename);
  return GURL(path.value());
}


// Waits for the test case to finish.
void NaClTest::WaitForFinish(const FilePath& filename,
                             const int wait_time) {
  GURL url = GetTestUrl(filename);
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  bool test_result = WaitUntilCookieValue(tab.get(),
                                          url,
                                          kTestCompleteCookie,
                                          action_timeout_ms(),
                                          wait_time,
                                          kTestCompleteSuccess);
  EXPECT_TRUE(test_result);
}

void NaClTest::RunTest(const FilePath& filename, int timeout) {
  GURL url = GetTestUrl(filename);
  NavigateToURL(url);
  WaitForFinish(filename, timeout);
}

void NaClTest::PrepareSrpcHwTest(FilePath test_root_dir) {
  FilePath srpc_hw_dir = test_root_dir.AppendASCII("srpc_hw");
  FilePath html_file = srpc_hw_dir.Append(kSrpcHwHtmlFileName);
  FilePath nexe_file = srpc_hw_dir.Append(kSrpcHwNexeFileName);
  ASSERT_TRUE(file_util::PathExists(html_file));
  ASSERT_TRUE(file_util::PathExists(nexe_file));
  // Now copy the files into the test directory
  ASSERT_TRUE(file_util::CopyFile(
      html_file,
      test_root_dir.Append(kSrpcHwHtmlFileName)));
  ASSERT_TRUE(file_util::CopyFile(
      nexe_file,
      test_root_dir.Append(kSrpcHwNexeFileName)));
}

void NaClTest::PrepareServerTest(FilePath test_root_dir) {
  FilePath srpc_hw_dir = test_root_dir.AppendASCII("server");
  FilePath html_file = srpc_hw_dir.Append(kServerHtmlFileName);
  ASSERT_TRUE(file_util::PathExists(html_file));
  // Now copy the files into the test directory
  ASSERT_TRUE(file_util::CopyFile(
      html_file,
      test_root_dir.Append(kServerHtmlFileName)));
}


void NaClTest::SetUp() {
  FilePath nacl_test_dir = GetTestRootDir();
  PrepareSrpcHwTest(nacl_test_dir);
  PrepareServerTest(nacl_test_dir);

  UITest::SetUp();

  StartHttpServerWithPort(nacl_test_dir, L"5103");
}

void NaClTest::TearDown() {
  StopHttpServer();
  UITest::TearDown();
}

TEST_F(NaClTest, ServerTest) {
  FilePath server_test_file(kServerHtmlFileName);
  RunTest(server_test_file, action_max_timeout_ms());
}


// http://crbug.com/30990
TEST_F(NaClTest, FLAKY_SrpcHelloWorld) {
  FilePath srpc_hw_file(kSrpcHwHtmlFileName);
  RunTest(srpc_hw_file, action_max_timeout_ms());
}

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/nacl_test.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

namespace {

const int kNaClTestTimeout = 20000;
const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";

const FilePath::CharType kBaseUrl[] =
    FILE_PATH_LITERAL("http://localhost:5103/");

const FilePath::CharType kSrpcHwHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_hw.html");
const FilePath::CharType kSrpcHwNexeFileName[] =
    FILE_PATH_LITERAL("srpc_hw.nexe");

const FilePath::CharType kSrpcBasicHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_basic.html");
const FilePath::CharType kSrpcBasicNexeFileName[] =
    FILE_PATH_LITERAL("srpc_test.nexe");

const FilePath::CharType kSrpcSockAddrHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_sockaddr.html");

const FilePath::CharType kSrpcShmHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_shm.html");
const FilePath::CharType kSrpcShmNexeFileName[] =
    FILE_PATH_LITERAL("srpc_shm.nexe");

const FilePath::CharType kSrpcPluginHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_plugin.html");

const FilePath::CharType kSrpcNrdXferHtmlFileName[] =
    FILE_PATH_LITERAL("srpc_nrd_xfer.html");
const FilePath::CharType kSrpcNrdClientNexeFileName[] =
    FILE_PATH_LITERAL("srpc_nrd_client.nexe");
const FilePath::CharType kSrpcNrdServerNexeFileName[] =
    FILE_PATH_LITERAL("srpc_nrd_server.nexe");

const FilePath::CharType kServerHtmlFileName[] =
    FILE_PATH_LITERAL("server_test.html");

}  // anonymous namespace

NaClTest::NaClTest()
    : UITest() {
  launch_arguments_.AppendSwitch(switches::kEnableNaCl);

  // Currently we disable some of the sandboxes.  See:
  // Make NaCl work in Chromium's Linux seccomp sandbox and the Mac sandbox
  // http://code.google.com/p/nativeclient/issues/detail?id=344
#if defined(OS_MACOSX)
  launch_arguments_.AppendSwitch(switches::kNoSandbox);
#elif defined(OS_LINUX) && defined(USE_SECCOMP_SANDBOX)
  launch_arguments_.AppendSwitch(switches::kDisableSeccompSandbox);
#endif
}

NaClTest::~NaClTest() {}

FilePath NaClTest::GetTestRootDir() {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("native_client");
  path = path.AppendASCII("tests");
  return path;
}

FilePath NaClTest::GetTestBinariesDir() {
  FilePath path = GetTestRootDir();
  path = path.AppendASCII("prebuilt");
  bool use_x64_nexes = false;
#if defined(OS_WIN)
  if (NaClOsIs64BitWindows())
    use_x64_nexes = true;
#elif defined(OS_LINUX) && defined(__LP64__)
  use_x64_nexes = true;
#endif

  if (use_x64_nexes)
    path = path.AppendASCII("x64");
  else
    path = path.AppendASCII("x86");
  return path;
}

// static
GURL NaClTest::GetTestUrl(const FilePath& filename) {
  FilePath path(kBaseUrl);
  path = path.Append(filename);
  return GURL(path.value());
}


void NaClTest::WaitForFinish(const FilePath& filename,
                             int wait_time) {
  GURL url = GetTestUrl(filename);
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  bool test_result = WaitUntilCookieValue(tab.get(),
                                          url,
                                          kTestCompleteCookie,
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
  FilePath test_dir = test_root_dir.AppendASCII("srpc_hw");
  FilePath html_file = test_dir.Append(kSrpcHwHtmlFileName);
  FilePath nexe_file = GetTestBinariesDir().Append(kSrpcHwNexeFileName);
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
  FilePath test_dir = test_root_dir.AppendASCII("server");
  FilePath html_file = test_dir.Append(kServerHtmlFileName);
  ASSERT_TRUE(file_util::PathExists(html_file));
  // Now copy the files into the test directory
  ASSERT_TRUE(file_util::CopyFile(
      html_file,
      test_root_dir.Append(kServerHtmlFileName)));
}

void NaClTest::PrepareSrpcBasicTest(FilePath test_root_dir) {
  FilePath test_dir = test_root_dir.AppendASCII("srpc");
  FilePath html_file = test_dir.Append(kSrpcBasicHtmlFileName);
  FilePath nexe_file = GetTestBinariesDir().Append(kSrpcBasicNexeFileName);
  ASSERT_TRUE(file_util::PathExists(html_file));
  ASSERT_TRUE(file_util::PathExists(nexe_file));
  // Now copy the files into the test directory
  ASSERT_TRUE(file_util::CopyFile(
      html_file,
      test_root_dir.Append(kSrpcBasicHtmlFileName)));
  ASSERT_TRUE(file_util::CopyFile(
      nexe_file,
      test_root_dir.Append(kSrpcBasicNexeFileName)));
}

void NaClTest::PrepareSrpcSockAddrTest(FilePath test_root_dir) {
  FilePath test_dir = test_root_dir.AppendASCII("srpc");
  FilePath html_file = test_dir.Append(kSrpcSockAddrHtmlFileName);
  FilePath nexe_file = GetTestBinariesDir().Append(kSrpcNrdServerNexeFileName);
  ASSERT_TRUE(file_util::PathExists(html_file));
  ASSERT_TRUE(file_util::PathExists(nexe_file));
  // Now copy the files into the test directory
  ASSERT_TRUE(file_util::CopyFile(
      html_file,
      test_root_dir.Append(kSrpcSockAddrHtmlFileName)));
  ASSERT_TRUE(file_util::CopyFile(
      nexe_file,
      test_root_dir.Append(kSrpcNrdServerNexeFileName)));
}

void NaClTest::PrepareSrpcShmTest(FilePath test_root_dir) {
  FilePath test_dir = test_root_dir.AppendASCII("srpc");
  FilePath html_file = test_dir.Append(kSrpcShmHtmlFileName);
  FilePath nexe_file = GetTestBinariesDir().Append(kSrpcShmNexeFileName);
  ASSERT_TRUE(file_util::PathExists(html_file));
  ASSERT_TRUE(file_util::PathExists(nexe_file));
  // Now copy the files into the test directory
  ASSERT_TRUE(file_util::CopyFile(
      html_file,
      test_root_dir.Append(kSrpcShmHtmlFileName)));
  ASSERT_TRUE(file_util::CopyFile(
      nexe_file,
      test_root_dir.Append(kSrpcShmNexeFileName)));
}

void NaClTest::PrepareSrpcPluginTest(FilePath test_root_dir) {
  FilePath test_dir = test_root_dir.AppendASCII("srpc");
  FilePath html_file = test_dir.Append(kSrpcPluginHtmlFileName);
  FilePath nexe_file1 = GetTestBinariesDir().Append(kSrpcNrdClientNexeFileName);
  FilePath nexe_file2 = GetTestBinariesDir().Append(kSrpcBasicNexeFileName);
  ASSERT_TRUE(file_util::PathExists(html_file));
  ASSERT_TRUE(file_util::PathExists(nexe_file1));
  ASSERT_TRUE(file_util::PathExists(nexe_file2));
  // Now copy the files into the test directory
  ASSERT_TRUE(file_util::CopyFile(
      html_file,
      test_root_dir.Append(kSrpcPluginHtmlFileName)));
  ASSERT_TRUE(file_util::CopyFile(
      nexe_file1,
      test_root_dir.Append(kSrpcNrdClientNexeFileName)));
  ASSERT_TRUE(file_util::CopyFile(
      nexe_file2,
      test_root_dir.Append(kSrpcBasicNexeFileName)));
}

void NaClTest::PrepareSrpcNrdXferTest(FilePath test_root_dir) {
  FilePath test_dir = test_root_dir.AppendASCII("srpc");
  FilePath html_file = test_dir.Append(kSrpcNrdXferHtmlFileName);
  FilePath nexe_file1 = GetTestBinariesDir().Append(kSrpcNrdClientNexeFileName);
  FilePath nexe_file2 = GetTestBinariesDir().Append(kSrpcNrdServerNexeFileName);
  ASSERT_TRUE(file_util::PathExists(html_file));
  ASSERT_TRUE(file_util::PathExists(nexe_file1));
  ASSERT_TRUE(file_util::PathExists(nexe_file2));
  // Now copy the files into the test directory
  ASSERT_TRUE(file_util::CopyFile(
      html_file,
      test_root_dir.Append(kSrpcNrdXferHtmlFileName)));
  ASSERT_TRUE(file_util::CopyFile(
      nexe_file1,
      test_root_dir.Append(kSrpcNrdClientNexeFileName)));
  ASSERT_TRUE(file_util::CopyFile(
      nexe_file2,
      test_root_dir.Append(kSrpcNrdServerNexeFileName)));
}

void NaClTest::SetUp() {
  FilePath nacl_test_dir = GetTestRootDir();
  PrepareSrpcHwTest(nacl_test_dir);
  PrepareServerTest(nacl_test_dir);
  PrepareSrpcBasicTest(nacl_test_dir);
  PrepareSrpcSockAddrTest(nacl_test_dir);
  PrepareSrpcShmTest(nacl_test_dir);
  PrepareSrpcPluginTest(nacl_test_dir);
  PrepareSrpcNrdXferTest(nacl_test_dir);

  UITest::SetUp();

  StartHttpServerWithPort(nacl_test_dir, L"5103");
}

void NaClTest::TearDown() {
  StopHttpServer();
  UITest::TearDown();
}

int NaClTest::NaClTestTimeout() {
  return std::max(kNaClTestTimeout, action_max_timeout_ms());
}

TEST_F(NaClTest, ServerTest) {
  FilePath test_file(kServerHtmlFileName);
  RunTest(test_file, NaClTestTimeout());
}

TEST_F(NaClTest, SrpcHelloWorld) {
  FilePath test_file(kSrpcHwHtmlFileName);
  RunTest(test_file, NaClTestTimeout());
}

TEST_F(NaClTest, SrpcBasicTest) {
  FilePath test_file(kSrpcBasicHtmlFileName);
  RunTest(test_file, NaClTestTimeout());
}

TEST_F(NaClTest, SrpcSockAddrTest) {
  FilePath test_file(kSrpcSockAddrHtmlFileName);
  RunTest(test_file, NaClTestTimeout());
}

TEST_F(NaClTest, SrpcShmTest) {
  FilePath test_file(kSrpcShmHtmlFileName);
  RunTest(test_file, NaClTestTimeout());
}

TEST_F(NaClTest, SrpcPluginTest) {
  FilePath test_file(kSrpcPluginHtmlFileName);
  RunTest(test_file, NaClTestTimeout());
}

TEST_F(NaClTest, SrpcNrdXferTest) {
  FilePath test_file(kSrpcNrdXferHtmlFileName);
  RunTest(test_file, NaClTestTimeout());
}

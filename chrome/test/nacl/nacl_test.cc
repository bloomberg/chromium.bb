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

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";

const FilePath::CharType kBaseUrl[] =
    FILE_PATH_LITERAL("http://localhost:5103/tests/prebuilt");

}  // namespace

NaClTest::NaClTest()
    : UITest(), use_x64_nexes_(false), multiarch_test_(false) {
  launch_arguments_.AppendSwitch(switches::kEnableNaCl);

  // Currently we disable some of the sandboxes.  See:
  // Make NaCl work in Chromium's Linux seccomp sandbox and the Mac sandbox
  // http://code.google.com/p/nativeclient/issues/detail?id=344
#if defined(OS_LINUX) && defined(USE_SECCOMP_SANDBOX)
  launch_arguments_.AppendSwitch(switches::kDisableSeccompSandbox);
#endif
  launch_arguments_.AppendSwitchASCII(switches::kLoggingLevel, "0");
}

NaClTest::~NaClTest() {}

FilePath NaClTest::GetTestRootDir() {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("native_client");
}

GURL NaClTest::GetTestUrl(const FilePath& filename) {
  FilePath path(kBaseUrl);
  // Multiarch tests are in the directory defined by kBaseUrl.
  if (!multiarch_test_) {
    if (use_x64_nexes_)
      path = path.AppendASCII("x64");
    else
      path = path.AppendASCII("x86");
  }
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

void NaClTest::RunMultiarchTest(const FilePath& filename, int timeout) {
  multiarch_test_ = true;
  RunTest(filename, timeout);
}

void NaClTest::SetUp() {
  FilePath nacl_test_dir = GetTestRootDir();
#if defined(OS_WIN)
  if (NaClOsIs64BitWindows())
    use_x64_nexes_ = true;
#elif defined(OS_LINUX) && defined(__LP64__)
  use_x64_nexes_ = true;
#endif

  UITest::SetUp();

  StartHttpServerWithPort(nacl_test_dir, 5103);
}

void NaClTest::TearDown() {
  StopHttpServer();
  UITest::TearDown();
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/nacl_ui_test.h"

// TODO(jvoung) see what includes we really need.
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "chrome/common/chrome_paths.h"

namespace {
// NOTE: The base URL of each HTML file is specified in nacl_test.
const FilePath::CharType kPPAPIHwHtmlFileName[] =
  FILE_PATH_LITERAL("srpc_hw_ppapi.html");
const FilePath::CharType kSrpcBasicHtmlFileName[] =
  FILE_PATH_LITERAL("srpc_basic.html");
const FilePath::CharType kSrpcSockAddrHtmlFileName[] =
  FILE_PATH_LITERAL("srpc_sockaddr.html");
const FilePath::CharType kSrpcShmHtmlFileName[] =
  FILE_PATH_LITERAL("srpc_shm.html");
const FilePath::CharType kSrpcPluginHtmlFileName[] =
  FILE_PATH_LITERAL("srpc_plugin.html");
const FilePath::CharType kSrpcNrdXferHtmlFileName[] =
  FILE_PATH_LITERAL("srpc_nrd_xfer.html");
const FilePath::CharType kServerHtmlFileName[] =
  FILE_PATH_LITERAL("server_test.html");
const FilePath::CharType kNpapiHwHtmlFileName[] =
  FILE_PATH_LITERAL("npapi_hw.html");
}  // namespace

NaClUITest::NaClUITest() {
}

NaClUITest::~NaClUITest() {
}

TEST_F(NaClUITest, ServerTest) {
  FilePath test_file(kServerHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NaClUITest, PPAPIHelloWorld) {
  FilePath test_file(kPPAPIHwHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}

// http://crbug.com/64973
TEST_F(NaClUITest, DISABLED_SrpcBasicTest) {
  FilePath test_file(kSrpcBasicHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NaClUITest, DISABLED_SrpcSockAddrTest) {
  FilePath test_file(kSrpcSockAddrHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NaClUITest, DISABLED_SrpcShmTest) {
  FilePath test_file(kSrpcShmHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NaClUITest, DISABLED_SrpcPluginTest) {
  FilePath test_file(kSrpcPluginHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NaClUITest, DISABLED_SrpcNrdXferTest) {
  FilePath test_file(kSrpcNrdXferHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}

TEST_F(NaClUITest, DISABLED_NpapiHwTest) {
  FilePath test_file(kNpapiHwHtmlFileName);
  RunTest(test_file, TestTimeouts::action_max_timeout_ms());
}

// TEST_F(NaClUITest, DISABLED_MultiarchTest) {
//   FilePath test_file(kSrpcHwHtmlFileName);
//   RunMultiarchTest(test_file, TestTimeouts::action_max_timeout_ms());
// }


// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/command_line.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "services/service_manager/sandbox/mac/common_v2.sb.h"
#include "services/service_manager/sandbox/mac/renderer_v2.sb.h"
#include "services/service_manager/sandbox/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace content {
namespace {

// crbug.com/740009: This allows the unit test to cleanup temporary directories,
// and is safe since this is only a unit test.
constexpr char kTempDirSuffix[] =
    "(allow file* (subpath \"/private/var/folders/\"))";

class SandboxMacTest : public base::MultiProcessTest {
 protected:
  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine cl = MultiProcessTest::MakeCmdLine(procname);
    cl.AppendArg(
        base::StringPrintf("%s%d", sandbox::switches::kSeatbeltClient, pipe_));
    return cl;
  }

  int pipe_{0};
};

void CheckCreateSeatbeltServer() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& argv = cl->argv();
  std::vector<char*> argv_cstr(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
    argv_cstr[i] = const_cast<char*>(argv[i].c_str());
  }
  auto result = sandbox::SeatbeltExecServer::CreateFromArguments(
      argv_cstr[0], argv_cstr.size(), argv_cstr.data());

  CHECK(result.sandbox_required);
  CHECK(result.server);
  CHECK(result.server->InitializeSandbox());
}

}  // namespace

MULTIPROCESS_TEST_MAIN(RendererWriteProcess) {
  CheckCreateSeatbeltServer();

  // Test that the renderer cannot write to the home directory.
  NSString* test_file = [NSHomeDirectory()
      stringByAppendingPathComponent:@"e539dd6f-6b38-4f6a-af2c-809a5ea96e1c"];
  int fd = HANDLE_EINTR(
      open(base::SysNSStringToUTF8(test_file).c_str(), O_CREAT | O_RDWR));
  CHECK(-1 == fd);
  CHECK_EQ(errno, EPERM);

  return 0;
}

TEST_F(SandboxMacTest, RendererCannotWriteHomeDir) {
  std::string profile =
      std::string(service_manager::kSeatbeltPolicyString_common_v2) +
      service_manager::kSeatbeltPolicyString_renderer_v2 + kTempDirSuffix;

  sandbox::SeatbeltExecClient client;
  client.SetProfile(profile);
  content::SetupCommonSandboxParameters(&client);

  pipe_ = client.GetReadFD();
  ASSERT_GE(pipe_, 0);

  base::LaunchOptions options;
  options.fds_to_remap.push_back(std::make_pair(pipe_, pipe_));

  base::Process process =
      SpawnChildWithOptions("RendererWriteProcess", options);
  ASSERT_TRUE(process.IsValid());
  ASSERT_TRUE(client.SendProfile());

  int rv = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      process, TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

}  // namespace content

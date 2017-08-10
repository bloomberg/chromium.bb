// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_type.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(SandboxTypeTest, Empty) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                 "network");
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, Renderer) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kRendererProcess);
  EXPECT_EQ(SANDBOX_TYPE_RENDERER, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                 "network");
  EXPECT_EQ(SANDBOX_TYPE_RENDERER, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, Utility) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kUtilityProcess);
  EXPECT_EQ(SANDBOX_TYPE_UTILITY, SandboxTypeFromCommandLine(command_line));

  base::CommandLine command_line2(command_line);
  command_line2.AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                  "network");
  EXPECT_EQ(SANDBOX_TYPE_NETWORK, SandboxTypeFromCommandLine(command_line2));

  base::CommandLine command_line3(command_line);
  command_line3.AppendSwitchASCII(switches::kUtilityProcessSandboxType, "none");
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line3));

  base::CommandLine command_line4(command_line);
  command_line4.AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                  "bogus");
  EXPECT_EQ(SANDBOX_TYPE_UTILITY, SandboxTypeFromCommandLine(command_line4));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, GPU) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);
  EXPECT_EQ(SANDBOX_TYPE_GPU, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                 "network");
  EXPECT_EQ(SANDBOX_TYPE_GPU, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, PPAPIBroker) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kPpapiBrokerProcess);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                 "network");
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, PPAPIPlugin) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kPpapiPluginProcess);
  EXPECT_EQ(SANDBOX_TYPE_PPAPI, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                 "network");
  EXPECT_EQ(SANDBOX_TYPE_PPAPI, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, Nonesuch) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType, "nonesuch");
  EXPECT_EQ(SANDBOX_TYPE_INVALID, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                 "network");
  EXPECT_EQ(SANDBOX_TYPE_INVALID, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SANDBOX_TYPE_NO_SANDBOX, SandboxTypeFromCommandLine(command_line));
}

}  // namespace content

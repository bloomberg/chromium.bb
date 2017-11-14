// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/allocator/features.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profiling {
namespace {

#if BUILDFLAG(USE_ALLOCATOR_SHIM)

TEST(ProfilingProcessHost, GetCurrentMode_Default) {
  EXPECT_EQ(ProfilingProcessHost::Mode::kNone,
            ProfilingProcessHost::GetCurrentMode());
}

TEST(ProfilingProcessHost, GetCurrentMode_Commandline) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kMemlog,
                                                              "");
    EXPECT_EQ(ProfilingProcessHost::Mode::kNone,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kMemlog,
                                                              "invalid");
    EXPECT_EQ(ProfilingProcessHost::Mode::kNone,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMemlog, switches::kMemlogModeAll);
    EXPECT_EQ(ProfilingProcessHost::Mode::kAll,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMemlog, switches::kMemlogModeBrowser);
    EXPECT_EQ(ProfilingProcessHost::Mode::kBrowser,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMemlog, switches::kMemlogModeMinimal);
    EXPECT_EQ(ProfilingProcessHost::Mode::kMinimal,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMemlog, switches::kMemlogModeGpu);
    EXPECT_EQ(ProfilingProcessHost::Mode::kGpu,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMemlog, switches::kMemlogModeRendererSampling);
    EXPECT_EQ(ProfilingProcessHost::Mode::kRendererSampling,
              ProfilingProcessHost::GetCurrentMode());
  }
}

TEST(ProfilingProcessHost, GetCurrentMode_Finch) {
  EXPECT_EQ(ProfilingProcessHost::Mode::kNone,
            ProfilingProcessHost::GetCurrentMode());
  std::map<std::string, std::string> parameters;

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[profiling::kOOPHeapProfilingFeatureMode] = "";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        profiling::kOOPHeapProfilingFeature, parameters);

    EXPECT_EQ(ProfilingProcessHost::Mode::kNone,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[profiling::kOOPHeapProfilingFeatureMode] = "invalid";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        profiling::kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(ProfilingProcessHost::Mode::kNone,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[profiling::kOOPHeapProfilingFeatureMode] =
        switches::kMemlogModeAll;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        profiling::kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(ProfilingProcessHost::Mode::kAll,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[profiling::kOOPHeapProfilingFeatureMode] =
        switches::kMemlogModeBrowser;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        profiling::kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(ProfilingProcessHost::Mode::kBrowser,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[profiling::kOOPHeapProfilingFeatureMode] =
        switches::kMemlogModeMinimal;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        profiling::kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(ProfilingProcessHost::Mode::kMinimal,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[profiling::kOOPHeapProfilingFeatureMode] =
        switches::kMemlogModeGpu;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        profiling::kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(ProfilingProcessHost::Mode::kGpu,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[profiling::kOOPHeapProfilingFeatureMode] =
        switches::kMemlogModeRendererSampling;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        profiling::kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(ProfilingProcessHost::Mode::kRendererSampling,
              ProfilingProcessHost::GetCurrentMode());
  }
}

// Ensure the commandline overrides any given field trial.
TEST(ProfilingProcessHost, GetCurrentMode_CommandLinePrecedence) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMemlog, switches::kMemlogModeAll);

  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> parameters;
  parameters[profiling::kOOPHeapProfilingFeatureMode] =
      switches::kMemlogModeMinimal;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      profiling::kOOPHeapProfilingFeature, parameters);

  EXPECT_EQ(ProfilingProcessHost::Mode::kAll,
            ProfilingProcessHost::GetCurrentMode());
}

#else

TEST(ProfilingProcessHost, GetCurrentMode_NoModeWithoutShim) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMemlog, switches::kMemlogModeAll);
    EXPECT_EQ(ProfilingProcessHost::Mode::kNone,
              ProfilingProcessHost::GetCurrentMode());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> parameters;
    parameters[profiling::kOOPHeapProfilingFeatureMode] =
        switches::kMemlogModeMinimal;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        profiling::kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(ProfilingProcessHost::Mode::kNone,
              ProfilingProcessHost::GetCurrentMode());
  }
}

#endif

}  // namespace
}  // namespace profiling

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/allocator/features.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profiling {

TEST(ProfilingProcessHost, ShouldProfileNewRenderer) {
  content::TestBrowserThreadBundle thread_bundle;

  ProfilingProcessHost pph;
  TestingProfile testing_profile;
  content::MockRenderProcessHost rph(&testing_profile);

  pph.SetMode(ProfilingProcessHost::Mode::kNone);
  EXPECT_FALSE(pph.ShouldProfileNewRenderer(&rph));

  pph.SetMode(ProfilingProcessHost::Mode::kAll);
  EXPECT_TRUE(pph.ShouldProfileNewRenderer(&rph));

  Profile* incognito_profile = testing_profile.GetOffTheRecordProfile();
  content::MockRenderProcessHost incognito_rph(incognito_profile);
  EXPECT_FALSE(pph.ShouldProfileNewRenderer(&incognito_rph));
}

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

}  // namespace profiling

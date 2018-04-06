// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/allocator/buildflags.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace heap_profiling {

TEST(ProfilingProcessHost, ShouldProfileNewRenderer) {
  content::TestBrowserThreadBundle thread_bundle;

  ProfilingProcessHost pph;
  TestingProfile testing_profile;
  content::MockRenderProcessHost rph(&testing_profile);

  pph.SetMode(Mode::kNone);
  EXPECT_FALSE(pph.ShouldProfileNewRenderer(&rph));

  pph.SetMode(Mode::kAll);
  EXPECT_TRUE(pph.ShouldProfileNewRenderer(&rph));

  Profile* incognito_profile = testing_profile.GetOffTheRecordProfile();
  content::MockRenderProcessHost incognito_rph(incognito_profile);
  EXPECT_FALSE(pph.ShouldProfileNewRenderer(&incognito_rph));
}

#if BUILDFLAG(USE_ALLOCATOR_SHIM)

TEST(ProfilingProcessHost, GetModeForStartup_Default) {
  EXPECT_EQ(Mode::kNone, GetModeForStartup());
}

TEST(ProfilingProcessHost, GetModeForStartup_Commandline) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlog, "");
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlog,
                                                              "invalid");
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlog,
                                                              kMemlogModeAll);
    EXPECT_EQ(Mode::kAll, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kMemlog, kMemlogModeBrowser);
    EXPECT_EQ(Mode::kBrowser, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kMemlog, kMemlogModeMinimal);
    EXPECT_EQ(Mode::kMinimal, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlog,
                                                              kMemlogModeGpu);
    EXPECT_EQ(Mode::kGpu, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kMemlog, kMemlogModeRendererSampling);
    EXPECT_EQ(Mode::kRendererSampling, GetModeForStartup());
  }
}

TEST(ProfilingProcessHost, GetModeForStartup_Finch) {
  EXPECT_EQ(Mode::kNone, GetModeForStartup());
  std::map<std::string, std::string> parameters;

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = "";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);

    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = "invalid";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeAll;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kAll, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeBrowser;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kBrowser, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeMinimal;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kMinimal, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeGpu;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kGpu, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeRendererSampling;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kRendererSampling, GetModeForStartup());
  }
}

// Ensure the commandline overrides any given field trial.
TEST(ProfilingProcessHost, GetModeForStartup_CommandLinePrecedence) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlog,
                                                            kMemlogModeAll);

  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeMinimal;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kOOPHeapProfilingFeature, parameters);

  EXPECT_EQ(Mode::kAll, GetModeForStartup());
}

#else

TEST(ProfilingProcessHost, GetModeForStartup_NoModeWithoutShim) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlog,
                                                              kMemlogModeAll);
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> parameters;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeMinimal;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }
}

#endif

}  // namespace heap_profiling

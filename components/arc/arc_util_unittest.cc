// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_util.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

// If an instance is created, based on the value passed to the consturctor,
// EnableARC feature is enabled/disabled in the scope.
class ScopedArcFeature {
 public:
  explicit ScopedArcFeature(bool enabled) {
    constexpr char kArcFeatureName[] = "EnableARC";
    if (enabled) {
      feature_list.InitFromCommandLine(kArcFeatureName, std::string());
    } else {
      feature_list.InitFromCommandLine(std::string(), kArcFeatureName);
    }
  }
  ~ScopedArcFeature() = default;

 private:
  base::test::ScopedFeatureList feature_list;
  DISALLOW_COPY_AND_ASSIGN(ScopedArcFeature);
};

using ArcUtilTest = testing::Test;

// Test --arc-available with EnableARC feature combination.
TEST_F(ArcUtilTest, IsArcAvailable_Installed) {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  // If ARC is not installed, IsArcAvailable() should return false,
  // regardless of EnableARC feature.
  command_line->InitFromArgv({""});

  // Not available, by-default.
  EXPECT_FALSE(IsArcAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_FALSE(IsArcAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
  }

  // If ARC is installed, IsArcAvailable() should return true when EnableARC
  // feature is set.
  command_line->InitFromArgv({"", "--arc-available"});

  // Not available, by-default, too.
  EXPECT_FALSE(IsArcAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_TRUE(IsArcAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
  }
}

TEST_F(ArcUtilTest, IsArcAvailable_OfficialSupport) {
  // Regardless of FeatureList, IsArcAvailable() should return true.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arc"});
  EXPECT_TRUE(IsArcAvailable());
}

// TODO(hidehiko): Add test for IsArcKioskMode().
// It depends on UserManager, but a utility to inject fake instance is
// available only in chrome/. To use it in components/, refactoring is needed.

TEST_F(ArcUtilTest, IsArcOptInVerificationDisabled) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({""});
  EXPECT_FALSE(IsArcOptInVerificationDisabled());

  command_line->InitFromArgv({"", "--disable-arc-opt-in-verification"});
  EXPECT_TRUE(IsArcOptInVerificationDisabled());
}

}  // namespace
}  // namespace arc

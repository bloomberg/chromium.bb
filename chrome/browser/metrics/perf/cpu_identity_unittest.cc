// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/cpu_identity.h"

#include <algorithm>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

TEST(CpuIdentityTest, IntelUarchTableIsSorted) {
  EXPECT_TRUE(std::is_sorted(
      internal::kIntelUarchTable,
      internal::kIntelUarchTableEnd,
      internal::IntelUarchTableCmp));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_IvyBridge) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x3a;  // IvyBridge
  cpuid.model_name = "";
  EXPECT_EQ("IvyBridge", GetIntelUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnUarch_SandyBridge) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x2a;  // SandyBridge
  cpuid.model_name = "";
  EXPECT_EQ("SandyBridge", GetIntelUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnArch_x86_32) {
  CPUIdentity cpuid;
  cpuid.arch = "x86";
  cpuid.vendor = "GenuineIntel";
  cpuid.family = 0x06;
  cpuid.model = 0x2f;  // Westmere
  cpuid.model_name = "";
  EXPECT_EQ("Westmere", GetIntelUarch(cpuid));
}

TEST(CpuIdentityTest, DefaultCommandsBasedOnArch_Unknown) {
  CPUIdentity cpuid;
  cpuid.arch = "x86_64";
  cpuid.vendor = "NotIntel";
  cpuid.family = 0;
  cpuid.model = 0;
  cpuid.model_name = "";
  EXPECT_EQ("", GetIntelUarch(cpuid));
}

TEST(CpuIdentityTest, SimplifyCPUModelName) {
  EXPECT_EQ("", SimplifyCPUModelName(""));
  EXPECT_EQ("intel-celeron-2955u-@-1.40ghz",
            SimplifyCPUModelName("Intel(R) Celeron(R) 2955U @ 1.40GHz"));
  EXPECT_EQ("armv7-processor-rev-3-(v7l)",
            SimplifyCPUModelName("ARMv7 Processor rev 3 (v7l)"));
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include <memory>

#include "base/strings/string_split.h"
#include "gpu/config/gpu_control_list_jsons.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

TEST(GpuUtilTest, MergeFeatureSets) {
  {
    // Merge two empty sets.
    std::set<int> src;
    std::set<int> dst;
    EXPECT_TRUE(dst.empty());
    MergeFeatureSets(&dst, src);
    EXPECT_TRUE(dst.empty());
  }
  {
    // Merge an empty set into a set with elements.
    std::set<int> src;
    std::set<int> dst;
    dst.insert(1);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(1u, dst.size());
  }
  {
    // Merge two sets where the source elements are already in the target set.
    std::set<int> src;
    std::set<int> dst;
    src.insert(1);
    dst.insert(1);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(1u, dst.size());
  }
  {
    // Merge two sets with different elements.
    std::set<int> src;
    std::set<int> dst;
    src.insert(1);
    dst.insert(2);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(2u, dst.size());
  }
}

TEST(GpuUtilTest, StringToFeatureSet) {
  {
    // zero feature.
    std::set<int> features;
    StringToFeatureSet("", &features);
    EXPECT_EQ(0u, features.size());
  }
  {
    // One features.
    std::set<int> features;
    StringToFeatureSet("4", &features);
    EXPECT_EQ(1u, features.size());
  }
  {
    // Multiple features.
    std::set<int> features;
    StringToFeatureSet("1,9", &features);
    EXPECT_EQ(2u, features.size());
  }
}

TEST(GpuUtilTest,
     ApplyGpuDriverBugWorkarounds_DisabledExtensions) {
  GPUInfo gpu_info;
  CollectBasicGraphicsInfo(&gpu_info);
  std::unique_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  list->LoadList(kGpuDriverBugListJson, GpuControlList::kCurrentOsOnly);
  list->MakeDecision(GpuControlList::kOsAny, std::string(), gpu_info);
  std::vector<std::string> expected_disabled_extensions =
      list->GetDisabledExtensions();
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ApplyGpuDriverBugWorkarounds(gpu_info, &command_line);

  std::vector<std::string> actual_disabled_extensions = base::SplitString(
      command_line.GetSwitchValueASCII(switches::kDisableGLExtensions), ", ;",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  sort(expected_disabled_extensions.begin(),
       expected_disabled_extensions.end());
  sort(actual_disabled_extensions.begin(), actual_disabled_extensions.end());

  EXPECT_EQ(expected_disabled_extensions, actual_disabled_extensions);
}

}  // namespace gpu

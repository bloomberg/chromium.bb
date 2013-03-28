// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_control_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MachineModelInfoTest : public testing::Test {
 public:
  MachineModelInfoTest() { }
  virtual ~MachineModelInfoTest() { }

  typedef GpuControlList::MachineModelInfo MachineModelInfo;
};

TEST_F(MachineModelInfoTest, ValidModelInfo) {
  const std::string name_op[] = {
    "contains",
    "beginwith",
    "endwith",
    "="
  };
  const std::string version_op[] = {
    "=",
    "<",
    "<=",
    ">",
    ">=",
    "any",
    "between"
  };
  for (size_t i = 0; i < arraysize(name_op); ++i) {
    for (size_t j = 0; j < arraysize(version_op); ++j) {
      std::string version1;
      std::string version2;
      if (version_op[j] != "any")
        version1 = "3.14";
      if (version_op[j] == "between")
        version2 = "5.4";
      MachineModelInfo info(name_op[i], "model",
                            version_op[j], version1, version2);
      EXPECT_TRUE(info.IsValid());
    }
  }
}

TEST_F(MachineModelInfoTest, ModelComparison) {
  MachineModelInfo info("=", "model_a",
                        ">", "3.4", "");
  EXPECT_TRUE(info.Contains("model_a", "4"));
  EXPECT_FALSE(info.Contains("model_b", "4"));
  EXPECT_FALSE(info.Contains("model_a", "3.2"));
}

}  // namespace content


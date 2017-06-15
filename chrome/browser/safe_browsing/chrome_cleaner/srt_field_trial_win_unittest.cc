// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/win/windows_version.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SRTDownloadURLTest : public ::testing::Test {
 protected:
  void SetUp() override {
    field_trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);
  }

  void CreatePromptTrial(const std::string& experiment_name,
                         const std::string& download_group) {
    std::map<std::string, std::string> params;
    if (!download_group.empty())
      params["download_group"] = download_group;

    // Assigned trials will go out of scope when field_trial_list_ goes out
    // of scope.
    constexpr char kTrialName[] = "SRTPromptFieldTrial";
    variations::AssociateVariationParams(kTrialName, experiment_name, params);
    base::FieldTrialList::CreateFieldTrial(kTrialName, experiment_name);
  }

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(SRTDownloadURLTest, Stable) {
  CreatePromptTrial("On", "");
  EXPECT_EQ("/dl/softwareremovaltool/win/chrome_cleanup_tool.exe",
            GetSRTDownloadURL().path());
}

TEST_F(SRTDownloadURLTest, Canary) {
  CreatePromptTrial("SRTCanary", "");
  EXPECT_EQ("/dl/softwareremovaltool/win/c/chrome_cleanup_tool.exe",
            GetSRTDownloadURL().path());
}

TEST_F(SRTDownloadURLTest, Experiment) {
  CreatePromptTrial("Experiment", "experiment");
  std::string expected_path;
  if (base::win::OSInfo::GetInstance()->architecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    expected_path =
        "/dl/softwareremovaltool/win/x86/experiment/chrome_cleanup_tool.exe";
  } else {
    expected_path =
        "/dl/softwareremovaltool/win/x64/experiment/chrome_cleanup_tool.exe";
  }
  EXPECT_EQ(expected_path, GetSRTDownloadURL().path());
}

TEST_F(SRTDownloadURLTest, DefaultsToStable) {
  EXPECT_EQ("/dl/softwareremovaltool/win/chrome_cleanup_tool.exe",
            GetSRTDownloadURL().path());
}

}  // namespace safe_browsing

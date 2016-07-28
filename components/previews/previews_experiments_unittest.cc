// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/previews_experiments.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using PreviewsExperimentsTest = testing::Test;

}  // namespace

namespace previews {

TEST_F(PreviewsExperimentsTest, TestFieldTrialOfflinePage) {
  EXPECT_FALSE(IsIncludedInClientSidePreviewsExperimentsFieldTrial());
  EXPECT_FALSE(IsOfflinePreviewsEnabled());

  std::map<std::string, std::string> params;
  params["show_offline_pages"] = "true";
  ASSERT_TRUE(variations::AssociateVariationParams("ClientSidePreviews",
                                                   "Enabled", params));
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(
      base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled"));

  EXPECT_TRUE(IsIncludedInClientSidePreviewsExperimentsFieldTrial());
  EXPECT_TRUE(IsOfflinePreviewsEnabled());
}

}  // namespace previews

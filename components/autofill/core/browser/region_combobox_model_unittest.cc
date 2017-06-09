// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/region_combobox_model.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_region_data_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"

namespace autofill {

// Strings used in more than one place and must be the same everywhere.
const char kQuebec[] = "Quebec";
const char kOntario[] = "Ontario";

// Make sure the two regions returned by the source are properly set in the
// model.
TEST(RegionComboboxModelTest, QuebecOntarioRegions) {
  TestRegionDataLoader test_region_data_loader;
  RegionComboboxModel model;
  model.LoadRegionData("", &test_region_data_loader, 0);

  std::vector<std::pair<std::string, std::string>> regions;
  regions.push_back(std::make_pair("QC", kQuebec));
  regions.push_back(std::make_pair("ON", kOntario));

  test_region_data_loader.SendAsynchronousData(regions);

  EXPECT_EQ(3, model.GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16("---"), model.GetItemAt(0));
  EXPECT_EQ(base::ASCIIToUTF16(kQuebec), model.GetItemAt(1));
  EXPECT_EQ(base::ASCIIToUTF16(kOntario), model.GetItemAt(2));
  EXPECT_FALSE(model.failed_to_load_data());
}

// Make sure the combo box properly support source emptyness/failures.
TEST(RegionComboboxModelTest, FailingSource) {
  TestRegionDataLoader test_region_data_loader;
  RegionComboboxModel model;
  model.LoadRegionData("", &test_region_data_loader, 0);
  test_region_data_loader.SendAsynchronousData(
      std::vector<std::pair<std::string, std::string>>());

  // There's always 1 item, even in failure cases.
  EXPECT_EQ(1, model.GetItemCount());
  EXPECT_TRUE(model.failed_to_load_data());
}

}  // namespace autofill

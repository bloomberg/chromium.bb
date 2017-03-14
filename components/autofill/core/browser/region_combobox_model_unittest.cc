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
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"

namespace autofill {

// Strings used in more than one place and must be the same everywhere.
const char kTestCountryCode[] = "CA";
const char kQuebec[] = "Quebec";
const char kOntario[] = "Ontario";

class RegionComboboxModelTest : public testing::Test {
 public:
  RegionComboboxModelTest()
      : pref_service_(autofill::test::PrefServiceForTesting()) {
    manager_.SetTestingPrefService(pref_service_.get());
    manager_.set_timezone_country_code(kTestCountryCode);
  }

  void SetupCombobox(bool source_failure) {
    model_.reset(new RegionComboboxModel(
        base::WrapUnique(new TestSource(source_failure)),
        base::WrapUnique(new ::i18n::addressinput::NullStorage),
        manager_.app_locale(), kTestCountryCode));
  }

  void TearDown() override { manager_.SetTestingPrefService(nullptr); }

  RegionComboboxModel* model() { return model_.get(); }

 private:
  // The source that returns the region data. Using
  // third_party/libaddressinput/src/cpp/test/testdata_source.h wouldn't help
  // much since it only implements the Get method overriden below anyway.
  class TestSource : public ::i18n::addressinput::Source {
   public:
    explicit TestSource(bool source_failure)
        : source_failure_(source_failure) {}
    ~TestSource() override {}

    void Get(const std::string& key,
             const ::i18n::addressinput::Source::Callback& data_ready)
        const override {
      if (source_failure_) {
        data_ready(false, key, nullptr);
        return;
      }
      // Only set the fields needed to fill the combobox, since only the
      // combobox code needs to be tested here.
      std::string* json = new std::string("{\"data/CA\":{");
      *json += "\"id\":\"data/CA\",";
      *json += "\"key\":\"CA\",";
      *json += "\"sub_keys\":\"QC~ON\"},";

      *json += "\"data/CA/ON\":{";
      *json += "\"id\":\"data/CA/ON\",";
      *json += "\"key\":\"ON\",";
      *json += "\"name\":\"Ontario\"},";

      *json += "\"data/CA/QC\":{";
      *json += "\"id\":\"data/CA/QC\",";
      *json += "\"key\":\"QC\",";
      *json += "\"name\":\"Quebec\"}}";
      data_ready(true, key, json);
    }

   private:
    bool source_failure_{false};
  };
  TestPersonalDataManager manager_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<RegionComboboxModel> model_;
};

// Make sure the two regions returned by the source are properly set in the
// model.
TEST_F(RegionComboboxModelTest, QuebecOntarioRegions) {
  SetupCombobox(false);
  EXPECT_EQ(2, model()->GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16(kQuebec), model()->GetItemAt(0));
  EXPECT_EQ(base::ASCIIToUTF16(kOntario), model()->GetItemAt(1));
}

// Make sure the combo box properly support source failures.
TEST_F(RegionComboboxModelTest, FailingSource) {
  SetupCombobox(true);
  EXPECT_EQ(0, model()->GetItemCount());
}

}  // namespace autofill

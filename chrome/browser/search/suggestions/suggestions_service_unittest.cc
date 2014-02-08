// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_service.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace suggestions {

class SuggestionsServiceTest : public testing::Test {
 protected:
  SuggestionsServiceTest() {
    profile_ = profile_builder_.Build();
  }
  virtual ~SuggestionsServiceTest() {}

  // Enables the "ChromeSuggestions.Group1" field trial.
  void EnableFieldTrial() {
    // Clear the existing |field_trial_list_| to avoid firing a DCHECK.
    field_trial_list_.reset(NULL);
    field_trial_list_.reset(
        new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        kSuggestionsFieldTrialName, "Group1");
    std::map<std::string, std::string> params;
    params[kSuggestionsFieldTrialStateParam] =
        kSuggestionsFieldTrialStateEnabled;
    chrome_variations::AssociateVariationParams(kSuggestionsFieldTrialName,
                                                "Group1", params);
    field_trial_->group();
  }

  SuggestionsService* CreateSuggestionsService() {
    SuggestionsServiceFactory* suggestions_service_factory =
      SuggestionsServiceFactory::GetInstance();
    return suggestions_service_factory->GetForProfile(profile_.get());
  }

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;
  scoped_refptr<base::FieldTrial> field_trial_;
  TestingProfile::Builder profile_builder_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceTest);
};

TEST_F(SuggestionsServiceTest, ServiceBeingCreated) {
  // Field trial not enabled.
  EXPECT_TRUE(CreateSuggestionsService() == NULL);

  // Field trial enabled.
  EnableFieldTrial();
  EXPECT_TRUE(CreateSuggestionsService() != NULL);
}

}  // namespace suggestions

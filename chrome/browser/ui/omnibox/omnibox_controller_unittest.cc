// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxControllerTest : public testing::Test {
 protected:
  OmniboxControllerTest();
  virtual ~OmniboxControllerTest();

  void CreateController();
  void AssertProviders(int expected_providers);

  PrefService* GetPrefs() { return profile_.GetPrefs(); }
  const ACProviders* GetAutocompleteProviders() const {
    return omnibox_controller_->autocomplete_controller()->providers();
  }

 private:
  TestingProfile profile_;
  scoped_ptr<OmniboxController> omnibox_controller_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxControllerTest);
};

OmniboxControllerTest::OmniboxControllerTest() {
}

OmniboxControllerTest::~OmniboxControllerTest() {
}

void OmniboxControllerTest::CreateController() {
  omnibox_controller_.reset(new OmniboxController(NULL, &profile_));
}

// Checks that the list of autocomplete providers used by the OmniboxController
// matches the one in the |expected_providers| bit field.
void OmniboxControllerTest::AssertProviders(int expected_providers) {
  const ACProviders* providers = GetAutocompleteProviders();

  for (size_t i = 0; i < providers->size(); ++i) {
    // Ensure this is a provider we wanted.
    int type = providers->at(i)->type();
    ASSERT_TRUE(expected_providers & type);

    // Remove it from expectations so we fail if it's there twice.
    expected_providers &= ~type;
  }

  // Ensure we saw all the providers we expected.
  ASSERT_EQ(0, expected_providers);
}

TEST_F(OmniboxControllerTest, CheckDefaultAutocompleteProviders) {
  CreateController();
  // First collect the basic providers.
  int observed_providers = 0;
  const ACProviders* providers = GetAutocompleteProviders();
  for (size_t i = 0; i < providers->size(); ++i)
    observed_providers |= providers->at(i)->type();
  // Ensure we have at least one provider.
  ASSERT_NE(0, observed_providers);

  // Ensure instant extended includes all the provides in classic Chrome.
  int providers_with_instant_extended = observed_providers;
  // TODO(beaudoin): remove TYPE_SEARCH once it's no longer needed to pass
  // the Instant suggestion through via FinalizeInstantQuery.
  chrome::EnableInstantExtendedAPIForTesting();
  CreateController();
  AssertProviders(providers_with_instant_extended);
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_auditor.h"

#include <memory>

#include "components/consent_auditor/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/user_events/fake_user_event_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace consent_auditor {

namespace {

const char kLocalConsentDescriptionKey[] = "description";
const char kLocalConsentConfirmationKey[] = "confirmation";
const char kLocalConsentVersionInfoKey[] = "version";

// Fake product version for testing.
const char kCurrentProductVersion[] = "1.2.3.4";

}  // namespace

class ConsentAuditorTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = base::MakeUnique<TestingPrefServiceSimple>();
    user_event_service_ = base::MakeUnique<syncer::FakeUserEventService>();
    ConsentAuditor::RegisterProfilePrefs(pref_service_->registry());
    consent_auditor_ = base::MakeUnique<ConsentAuditor>(
        pref_service_.get(), user_event_service_.get(), kCurrentProductVersion);
  }

  void UpdateProductVersion(const std::string& new_product_version) {
    // We'll have to recreate |consent_auditor| in order to update
    // a new version. This is not a problem, as in reality we'd have to restart
    // Chrome to update the version, let alone just recreate this class.
    consent_auditor_ = base::MakeUnique<ConsentAuditor>(
        pref_service_.get(), user_event_service_.get(), new_product_version);
  }

  ConsentAuditor* consent_auditor() { return consent_auditor_.get(); }

  PrefService* pref_service() const { return pref_service_.get(); }

 private:
  std::unique_ptr<ConsentAuditor> consent_auditor_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<syncer::FakeUserEventService> user_event_service_;
};

TEST_F(ConsentAuditorTest, LocalConsentPrefRepresentation) {
  // No consents are written at first.
  EXPECT_FALSE(pref_service()->HasPrefPath(prefs::kLocalConsentsDictionary));

  // Record a consent and check that it appears in the prefs.
  const std::string kFeature1Description = "This will enable feature 1.";
  const std::string kFeature1Confirmation = "OK.";
  consent_auditor()->RecordLocalConsent("feature1", kFeature1Description,
                                        kFeature1Confirmation);
  ASSERT_TRUE(pref_service()->HasPrefPath(prefs::kLocalConsentsDictionary));
  const base::DictionaryValue* consents =
      pref_service()->GetDictionary(prefs::kLocalConsentsDictionary);
  ASSERT_TRUE(consents);
  const base::Value* record =
      consents->FindKeyOfType("feature1", base::Value::Type::DICTIONARY);
  ASSERT_TRUE(record);
  const base::Value* description = record->FindKey(kLocalConsentDescriptionKey);
  const base::Value* confirmation =
      record->FindKey(kLocalConsentConfirmationKey);
  const base::Value* version = record->FindKey(kLocalConsentVersionInfoKey);
  ASSERT_TRUE(description);
  ASSERT_TRUE(confirmation);
  ASSERT_TRUE(version);
  EXPECT_EQ(description->GetString(), kFeature1Description);
  EXPECT_EQ(confirmation->GetString(), kFeature1Confirmation);
  EXPECT_EQ(version->GetString(), kCurrentProductVersion);

  // Do the same for another feature.
  const std::string kFeature2Description = "Enable feature 2?";
  const std::string kFeature2Confirmation = "Yes.";
  consent_auditor()->RecordLocalConsent("feature2", kFeature2Description,
                                        kFeature2Confirmation);
  record = consents->FindKeyOfType("feature2", base::Value::Type::DICTIONARY);
  ASSERT_TRUE(record);
  description = record->FindKey(kLocalConsentDescriptionKey);
  confirmation = record->FindKey(kLocalConsentConfirmationKey);
  version = record->FindKey(kLocalConsentVersionInfoKey);
  ASSERT_TRUE(description);
  ASSERT_TRUE(confirmation);
  ASSERT_TRUE(version);
  EXPECT_EQ(description->GetString(), kFeature2Description);
  EXPECT_EQ(confirmation->GetString(), kFeature2Confirmation);
  EXPECT_EQ(version->GetString(), kCurrentProductVersion);

  // They are two separate records; the latter did not overwrite the former.
  EXPECT_EQ(2u, consents->size());
  EXPECT_TRUE(
      consents->FindKeyOfType("feature1", base::Value::Type::DICTIONARY));

  // Overwrite an existing consent and use a different product version.
  const std::string kFeature2NewDescription = "Re-enable feature 2?";
  const std::string kFeature2NewConfirmation = "Yes again.";
  const std::string kFeature2NewProductVersion = "5.6.7.8";
  UpdateProductVersion(kFeature2NewProductVersion);
  consent_auditor()->RecordLocalConsent("feature2", kFeature2NewDescription,
                                        kFeature2NewConfirmation);
  record = consents->FindKeyOfType("feature2", base::Value::Type::DICTIONARY);
  ASSERT_TRUE(record);
  description = record->FindKey(kLocalConsentDescriptionKey);
  confirmation = record->FindKey(kLocalConsentConfirmationKey);
  version = record->FindKey(kLocalConsentVersionInfoKey);
  ASSERT_TRUE(description);
  ASSERT_TRUE(confirmation);
  ASSERT_TRUE(version);
  EXPECT_EQ(description->GetString(), kFeature2NewDescription);
  EXPECT_EQ(confirmation->GetString(), kFeature2NewConfirmation);
  EXPECT_EQ(version->GetString(), kFeature2NewProductVersion);

  // We still have two records.
  EXPECT_EQ(2u, consents->size());
}

}  // namespace consent_auditor

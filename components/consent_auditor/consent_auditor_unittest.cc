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
const char kLocalConsentVersionKey[] = "version";
const char kLocalConsentLocaleKey[] = "locale";

// Fake product version for testing.
const char kCurrentAppVersion[] = "1.2.3.4";
const char kCurrentAppLocale[] = "en-US";

// A helper function to load the |description|, |confirmation|, |version|,
// and |locale|, in that order, from a record for the |feature| in
// the |consents| dictionary.
void LoadEntriesFromLocalConsentRecord(const base::Value* consents,
                                       const std::string& feature,
                                       std::string* description,
                                       std::string* confirmation,
                                       std::string* version,
                                       std::string* locale) {
  SCOPED_TRACE(::testing::Message() << "|feature| = " << feature);

  const base::Value* record =
      consents->FindKeyOfType(feature, base::Value::Type::DICTIONARY);
  ASSERT_TRUE(record);
  SCOPED_TRACE(::testing::Message() << "|record| = " << record);

  const base::Value* description_entry =
      record->FindKey(kLocalConsentDescriptionKey);
  const base::Value* confirmation_entry =
      record->FindKey(kLocalConsentConfirmationKey);
  const base::Value* version_entry = record->FindKey(kLocalConsentVersionKey);
  const base::Value* locale_entry = record->FindKey(kLocalConsentLocaleKey);

  ASSERT_TRUE(description_entry);
  ASSERT_TRUE(confirmation_entry);
  ASSERT_TRUE(version_entry);
  ASSERT_TRUE(locale_entry);

  *description = description_entry->GetString();
  *confirmation = confirmation_entry->GetString();
  *version = version_entry->GetString();
  *locale = locale_entry->GetString();
}

}  // namespace

class ConsentAuditorTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = base::MakeUnique<TestingPrefServiceSimple>();
    user_event_service_ = base::MakeUnique<syncer::FakeUserEventService>();
    ConsentAuditor::RegisterProfilePrefs(pref_service_->registry());
    consent_auditor_ = base::MakeUnique<ConsentAuditor>(
        pref_service_.get(), user_event_service_.get(), kCurrentAppVersion,
        kCurrentAppLocale);
  }

  void UpdateAppVersionAndLocale(const std::string& new_product_version,
                                 const std::string& new_app_locale) {
    // We'll have to recreate |consent_auditor| in order to update the version
    // and locale. This is not a problem, as in reality we'd have to restart
    // Chrome to update both, let alone just recreate this class.
    consent_auditor_ = base::MakeUnique<ConsentAuditor>(
        pref_service_.get(), user_event_service_.get(), new_product_version,
        new_app_locale);
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
  std::string description;
  std::string confirmation;
  std::string version;
  std::string locale;
  LoadEntriesFromLocalConsentRecord(consents, "feature1", &description,
                                    &confirmation, &version, &locale);
  EXPECT_EQ(kFeature1Description, description);
  EXPECT_EQ(kFeature1Confirmation, confirmation);
  EXPECT_EQ(kCurrentAppVersion, version);
  EXPECT_EQ(kCurrentAppLocale, locale);

  // Do the same for another feature.
  const std::string kFeature2Description = "Enable feature 2?";
  const std::string kFeature2Confirmation = "Yes.";
  consent_auditor()->RecordLocalConsent("feature2", kFeature2Description,
                                        kFeature2Confirmation);
  LoadEntriesFromLocalConsentRecord(consents, "feature2", &description,
                                    &confirmation, &version, &locale);
  EXPECT_EQ(kFeature2Description, description);
  EXPECT_EQ(kFeature2Confirmation, confirmation);
  EXPECT_EQ(kCurrentAppVersion, version);
  EXPECT_EQ(kCurrentAppLocale, locale);

  // They are two separate records; the latter did not overwrite the former.
  EXPECT_EQ(2u, consents->size());
  EXPECT_TRUE(
      consents->FindKeyOfType("feature1", base::Value::Type::DICTIONARY));

  // Overwrite an existing consent, this time use a different product version
  // and a different locale.
  const std::string kFeature2NewDescription = "Re-enable feature 2?";
  const std::string kFeature2NewConfirmation = "Yes again.";
  const std::string kFeature2NewAppVersion = "5.6.7.8";
  const std::string kFeature2NewAppLocale = "de";
  UpdateAppVersionAndLocale(kFeature2NewAppVersion, kFeature2NewAppLocale);
  consent_auditor()->RecordLocalConsent("feature2", kFeature2NewDescription,
                                        kFeature2NewConfirmation);
  LoadEntriesFromLocalConsentRecord(consents, "feature2", &description,
                                    &confirmation, &version, &locale);
  EXPECT_EQ(kFeature2NewDescription, description);
  EXPECT_EQ(kFeature2NewConfirmation, confirmation);
  EXPECT_EQ(kFeature2NewAppVersion, version);
  EXPECT_EQ(kFeature2NewAppLocale, locale);

  // We still have two records.
  EXPECT_EQ(2u, consents->size());
}

}  // namespace consent_auditor

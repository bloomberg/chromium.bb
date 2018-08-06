// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_auditor_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/sha1.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "components/consent_auditor/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/fake_model_type_controller_delegate.h"
#include "components/sync/user_events/fake_user_event_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using SyncConsent = sync_pb::UserConsentTypes::SyncConsent;
using sync_pb::UserConsentSpecifics;
using sync_pb::UserEventSpecifics;
using sync_pb::UserConsentTypes;

namespace consent_auditor {

namespace {

const char kLocalConsentDescriptionKey[] = "description";
const char kLocalConsentConfirmationKey[] = "confirmation";
const char kLocalConsentVersionKey[] = "version";
const char kLocalConsentLocaleKey[] = "locale";

// Fake product version for testing.
const char kCurrentAppVersion[] = "1.2.3.4";
const char kCurrentAppLocale[] = "en-US";

// Fake account ID for testing.
const char kAccountId[] = "testing_account_id";

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

class FakeConsentSyncBridge : public syncer::ConsentSyncBridge {
 public:
  ~FakeConsentSyncBridge() override = default;

  // ConsentSyncBridge implementation.
  void RecordConsent(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics) override {
    DCHECK(specifics);
    recorded_user_consents_.push_back(*specifics);
  }

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override {
    return delegate_;
  }

  // Fake methods.
  void SetControllerDelegate(
      base::WeakPtr<syncer::ModelTypeControllerDelegate> delegate) {
    delegate_ = delegate;
  }

  const std::vector<UserConsentSpecifics>& GetRecordedUserConsents() const {
    return recorded_user_consents_;
  }

 private:
  base::WeakPtr<syncer::ModelTypeControllerDelegate> delegate_;
  std::vector<UserConsentSpecifics> recorded_user_consents_;
};

}  // namespace

class ConsentAuditorImplTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    // Use normal clock by default.
    clock_ = base::DefaultClock::GetInstance();
    if (base::FeatureList::IsEnabled(switches::kSyncUserConsentSeparateType)) {
      consent_sync_bridge_ = std::make_unique<FakeConsentSyncBridge>();
    } else {
      user_event_service_ = std::make_unique<syncer::FakeUserEventService>();
    }
    ConsentAuditorImpl::RegisterProfilePrefs(pref_service_->registry());
    app_version_ = kCurrentAppVersion;
    app_locale_ = kCurrentAppLocale;
    BuildConsentAuditorImpl();
  }

  // TODO(vitaliii): Add a real builder class instead.
  void BuildConsentAuditorImpl() {
    consent_auditor_ = std::make_unique<ConsentAuditorImpl>(
        pref_service_.get(), std::move(consent_sync_bridge_),
        user_event_service_.get(), app_version_, app_locale_, clock_);
  }

  // These have no effect before |BuildConsentAuditorImpl|.
  void SetAppVersion(const std::string& new_app_version) {
    app_version_ = new_app_version;
  }
  void SetAppLocale(const std::string& new_app_locale) {
    app_locale_ = new_app_locale;
  }
  void SetConsentSyncBridge(std::unique_ptr<syncer::ConsentSyncBridge> bridge) {
    consent_sync_bridge_ = std::move(bridge);
  }
  void SetUserEventService(
      std::unique_ptr<syncer::FakeUserEventService> service) {
    user_event_service_ = std::move(service);
  }
  void SetClock(base::Clock* clock) { clock_ = clock; }

  void SetIsSeparateConsentTypeEnabledFeature(bool new_value) {
    feature_list_.InitWithFeatureState(switches::kSyncUserConsentSeparateType,
                                       new_value);
  }

  ConsentAuditorImpl* consent_auditor() { return consent_auditor_.get(); }
  PrefService* pref_service() const { return pref_service_.get(); }
  syncer::FakeUserEventService* user_event_service() {
    return user_event_service_.get();
  }

 private:
  std::unique_ptr<ConsentAuditorImpl> consent_auditor_;
  base::Clock* clock_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<syncer::FakeUserEventService> user_event_service_;
  std::string app_version_;
  std::string app_locale_;
  std::unique_ptr<syncer::ConsentSyncBridge> consent_sync_bridge_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ConsentAuditorImplTest, LocalConsentPrefRepresentation) {
  SetIsSeparateConsentTypeEnabledFeature(true);
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetConsentSyncBridge(std::make_unique<FakeConsentSyncBridge>());
  SetUserEventService(nullptr);
  BuildConsentAuditorImpl();

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
  SetAppVersion(kFeature2NewAppVersion);
  SetAppLocale(kFeature2NewAppLocale);
  SetConsentSyncBridge(std::make_unique<FakeConsentSyncBridge>());
  SetUserEventService(nullptr);
  // We rebuild consent auditor to emulate restarting Chrome. This is the only
  // way to change app version or app locale.
  BuildConsentAuditorImpl();

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

TEST_F(ConsentAuditorImplTest, RecordingEnabled) {
  SetIsSeparateConsentTypeEnabledFeature(false);
  SetConsentSyncBridge(nullptr);
  SetUserEventService(std::make_unique<syncer::FakeUserEventService>());
  BuildConsentAuditorImpl();

  SyncConsent sync_consent;
  sync_consent.set_status(UserConsentTypes::GIVEN);

  consent_auditor()->RecordSyncConsent(kAccountId, sync_consent);
  auto& events = user_event_service()->GetRecordedUserEvents();
  EXPECT_EQ(1U, events.size());
}

TEST_F(ConsentAuditorImplTest, RecordingDisabled) {
  SetIsSeparateConsentTypeEnabledFeature(false);
  SetConsentSyncBridge(nullptr);
  SetUserEventService(std::make_unique<syncer::FakeUserEventService>());
  BuildConsentAuditorImpl();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(switches::kSyncUserConsentEvents);
  SyncConsent sync_consent;
  sync_consent.set_status(UserConsentTypes::GIVEN);
  consent_auditor()->RecordSyncConsent(kAccountId, sync_consent);
  auto& events = user_event_service()->GetRecordedUserEvents();
  EXPECT_EQ(0U, events.size());
}

TEST_F(ConsentAuditorImplTest, RecordGaiaConsentAsUserEvent) {
  base::SimpleTestClock test_clock;

  SetIsSeparateConsentTypeEnabledFeature(false);
  SetConsentSyncBridge(nullptr);
  SetUserEventService(std::make_unique<syncer::FakeUserEventService>());
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetClock(&test_clock);
  BuildConsentAuditorImpl();

  std::vector<int> kDescriptionMessageIds = {12, 37, 42};
  int kConfirmationMessageId = 47;

  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  test_clock.SetNow(now);

  SyncConsent sync_consent;
  sync_consent.set_status(UserConsentTypes::GIVEN);
  sync_consent.set_confirmation_grd_id(kConfirmationMessageId);
  for (int id : kDescriptionMessageIds) {
    sync_consent.add_description_grd_ids(id);
  }
  consent_auditor()->RecordSyncConsent(kAccountId, sync_consent);

  auto& events = user_event_service()->GetRecordedUserEvents();
  EXPECT_EQ(1U, events.size());
  EXPECT_EQ(now.since_origin().InMicroseconds(), events[0].event_time_usec());
  EXPECT_FALSE(events[0].has_navigation_id());
  EXPECT_TRUE(events[0].has_user_consent());
  auto& consent = events[0].user_consent();
  EXPECT_EQ(kAccountId, consent.account_id());
  EXPECT_EQ(UserEventSpecifics::UserConsent::CHROME_SYNC, consent.feature());
  EXPECT_EQ(3, consent.description_grd_ids_size());
  EXPECT_EQ(kDescriptionMessageIds[0], consent.description_grd_ids(0));
  EXPECT_EQ(kDescriptionMessageIds[1], consent.description_grd_ids(1));
  EXPECT_EQ(kDescriptionMessageIds[2], consent.description_grd_ids(2));
  EXPECT_EQ(kConfirmationMessageId, consent.confirmation_grd_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());
}

TEST_F(ConsentAuditorImplTest, RecordGaiaConsentAsUserConsent) {
  SetIsSeparateConsentTypeEnabledFeature(true);

  auto wrapped_fake_bridge = std::make_unique<FakeConsentSyncBridge>();
  FakeConsentSyncBridge* fake_bridge = wrapped_fake_bridge.get();
  base::SimpleTestClock test_clock;

  SetConsentSyncBridge(std::move(wrapped_fake_bridge));
  SetUserEventService(nullptr);
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetClock(&test_clock);
  BuildConsentAuditorImpl();

  std::vector<int> kDescriptionMessageIds = {12, 37, 42};
  int kConfirmationMessageId = 47;

  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  test_clock.SetNow(now);

  SyncConsent sync_consent;
  sync_consent.set_status(UserConsentTypes::GIVEN);
  sync_consent.set_confirmation_grd_id(kConfirmationMessageId);
  for (int id : kDescriptionMessageIds) {
    sync_consent.add_description_grd_ids(id);
  }
  consent_auditor()->RecordSyncConsent(kAccountId, sync_consent);

  std::vector<UserConsentSpecifics> consents =
      fake_bridge->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  UserConsentSpecifics consent = consents[0];

  EXPECT_EQ(now.since_origin().InMicroseconds(),
            consent.client_consent_time_usec());
  EXPECT_EQ(kAccountId, consent.account_id());
  EXPECT_EQ(UserConsentSpecifics::CHROME_SYNC, consent.feature());
  EXPECT_EQ(3, consent.description_grd_ids_size());
  EXPECT_EQ(kDescriptionMessageIds[0], consent.description_grd_ids(0));
  EXPECT_EQ(kDescriptionMessageIds[1], consent.description_grd_ids(1));
  EXPECT_EQ(kDescriptionMessageIds[2], consent.description_grd_ids(2));
  EXPECT_EQ(kConfirmationMessageId, consent.confirmation_grd_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());
}

TEST_F(ConsentAuditorImplTest, RecordArcPlayConsentRevocation) {
  SetIsSeparateConsentTypeEnabledFeature(true);

  auto wrapped_fake_bridge = std::make_unique<FakeConsentSyncBridge>();
  FakeConsentSyncBridge* fake_bridge = wrapped_fake_bridge.get();
  base::SimpleTestClock test_clock;

  SetConsentSyncBridge(std::move(wrapped_fake_bridge));
  SetUserEventService(nullptr);
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetClock(&test_clock);
  BuildConsentAuditorImpl();

  std::vector<int> kDescriptionMessageIds = {12, 37, 42};
  int kConfirmationMessageId = 47;

  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  test_clock.SetNow(now);

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(UserConsentTypes::NOT_GIVEN);
  play_consent.set_confirmation_grd_id(kConfirmationMessageId);
  for (int id : kDescriptionMessageIds) {
    play_consent.add_description_grd_ids(id);
  }
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETTING_CHANGE);
  consent_auditor()->RecordArcPlayConsent(kAccountId, play_consent);

  std::vector<UserConsentSpecifics> consents =
      fake_bridge->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  UserConsentSpecifics consent = consents[0];

  EXPECT_EQ(kAccountId, consent.account_id());
  EXPECT_EQ(UserConsentTypes::NOT_GIVEN, consent.status());
  EXPECT_EQ(UserConsentSpecifics::PLAY_STORE, consent.feature());
  EXPECT_EQ(3, consent.description_grd_ids_size());
  EXPECT_EQ(kDescriptionMessageIds[0], consent.description_grd_ids(0));
  EXPECT_EQ(kDescriptionMessageIds[1], consent.description_grd_ids(1));
  EXPECT_EQ(kDescriptionMessageIds[2], consent.description_grd_ids(2));
  EXPECT_EQ(kConfirmationMessageId, consent.confirmation_grd_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());
}

TEST_F(ConsentAuditorImplTest, RecordArcPlayConsent) {
  SetIsSeparateConsentTypeEnabledFeature(true);

  auto wrapped_fake_bridge = std::make_unique<FakeConsentSyncBridge>();
  FakeConsentSyncBridge* fake_bridge = wrapped_fake_bridge.get();
  base::SimpleTestClock test_clock;

  SetConsentSyncBridge(std::move(wrapped_fake_bridge));
  SetUserEventService(nullptr);
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetClock(&test_clock);
  BuildConsentAuditorImpl();

  int kConfirmationMessageId = 47;

  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  test_clock.SetNow(now);

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(UserConsentTypes::GIVEN);
  play_consent.set_confirmation_grd_id(kConfirmationMessageId);
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETUP);

  // Verify the hash: 2fd4e1c6 7a2d28fc ed849ee1 bb76e739 1b93eb12.
  const char play_tos_hash[] = {0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28,
                                0xfc, 0xed, 0x84, 0x9e, 0xe1, 0xbb, 0x76,
                                0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12};
  play_consent.set_play_terms_of_service_hash(
      std::string(play_tos_hash, base::kSHA1Length));
  play_consent.set_play_terms_of_service_text_length(7);

  consent_auditor()->RecordArcPlayConsent(kAccountId, play_consent);

  std::vector<UserConsentSpecifics> consents =
      fake_bridge->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  UserConsentSpecifics consent = consents[0];

  EXPECT_EQ(kAccountId, consent.account_id());
  EXPECT_EQ(UserConsentSpecifics::PLAY_STORE, consent.feature());

  EXPECT_EQ(6, consent.description_grd_ids_size());
  EXPECT_EQ(7, consent.description_grd_ids(0));
  EXPECT_EQ(static_cast<int>(0x2fd4e1c6), consent.description_grd_ids(1));
  EXPECT_EQ(static_cast<int>(0x7a2d28fc), consent.description_grd_ids(2));
  EXPECT_EQ(static_cast<int>(0xed849ee1), consent.description_grd_ids(3));
  EXPECT_EQ(static_cast<int>(0xbb76e739), consent.description_grd_ids(4));
  EXPECT_EQ(static_cast<int>(0x1b93eb12), consent.description_grd_ids(5));

  EXPECT_EQ(kConfirmationMessageId, consent.confirmation_grd_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());
}

TEST_F(ConsentAuditorImplTest, ShouldReturnNoSyncDelegateWhenNoBridge) {
  SetIsSeparateConsentTypeEnabledFeature(false);
  SetConsentSyncBridge(nullptr);
  SetUserEventService(std::make_unique<syncer::FakeUserEventService>());
  BuildConsentAuditorImpl();

  // There is no bridge (i.e. separate sync type for consents is disabled),
  // thus, there should be no delegate as well.
  EXPECT_EQ(nullptr, consent_auditor()->GetControllerDelegate());
}

TEST_F(ConsentAuditorImplTest, ShouldReturnSyncDelegateWhenBridgePresent) {
  SetIsSeparateConsentTypeEnabledFeature(true);
  auto fake_bridge = std::make_unique<FakeConsentSyncBridge>();

  syncer::FakeModelTypeControllerDelegate fake_delegate(
      syncer::ModelType::USER_CONSENTS);
  auto expected_delegate_ptr = fake_delegate.GetWeakPtr();
  DCHECK(expected_delegate_ptr);
  fake_bridge->SetControllerDelegate(expected_delegate_ptr);

  SetConsentSyncBridge(std::move(fake_bridge));
  SetUserEventService(nullptr);
  BuildConsentAuditorImpl();

  // There is a bridge (i.e. separate sync type for consents is enabled), thus,
  // there should be a delegate as well.
  EXPECT_EQ(expected_delegate_ptr.get(),
            consent_auditor()->GetControllerDelegate().get());
}

}  // namespace consent_auditor

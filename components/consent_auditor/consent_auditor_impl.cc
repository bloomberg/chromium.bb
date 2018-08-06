// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_auditor_impl.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/values.h"
#include "components/consent_auditor/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/user_events/user_event_service.h"

using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using sync_pb::UserConsentTypes;
using sync_pb::UserConsentSpecifics;
using sync_pb::UserEventSpecifics;

namespace consent_auditor {

namespace {

const char kLocalConsentDescriptionKey[] = "description";
const char kLocalConsentConfirmationKey[] = "confirmation";
const char kLocalConsentVersionKey[] = "version";
const char kLocalConsentLocaleKey[] = "locale";

bool IsSeparateConsentTypeEnabled() {
  return base::FeatureList::IsEnabled(switches::kSyncUserConsentSeparateType);
}

UserEventSpecifics::UserConsent::Feature FeatureToUserEventProtoEnum(
    consent_auditor::Feature feature) {
  switch (feature) {
    case consent_auditor::Feature::CHROME_SYNC:
      return UserEventSpecifics::UserConsent::CHROME_SYNC;
    case consent_auditor::Feature::PLAY_STORE:
      return UserEventSpecifics::UserConsent::PLAY_STORE;
    case consent_auditor::Feature::BACKUP_AND_RESTORE:
      return UserEventSpecifics::UserConsent::BACKUP_AND_RESTORE;
    case consent_auditor::Feature::GOOGLE_LOCATION_SERVICE:
      return UserEventSpecifics::UserConsent::GOOGLE_LOCATION_SERVICE;
    case consent_auditor::Feature::CHROME_UNIFIED_CONSENT:
      return UserEventSpecifics::UserConsent::CHROME_UNIFIED_CONSENT;
  }
  NOTREACHED();
  return UserEventSpecifics::UserConsent::FEATURE_UNSPECIFIED;
}

UserConsentTypes::ConsentStatus StatusToProtoEnum(
    consent_auditor::ConsentStatus status) {
  switch (status) {
    case consent_auditor::ConsentStatus::NOT_GIVEN:
      return UserConsentTypes::NOT_GIVEN;
    case consent_auditor::ConsentStatus::GIVEN:
      return UserConsentTypes::GIVEN;
  }
  NOTREACHED();
  return UserConsentTypes::CONSENT_STATUS_UNSPECIFIED;
}

UserConsentSpecifics::Feature FeatureToUserConsentProtoEnum(
    consent_auditor::Feature feature) {
  switch (feature) {
    case consent_auditor::Feature::CHROME_SYNC:
      return UserConsentSpecifics::CHROME_SYNC;
    case consent_auditor::Feature::PLAY_STORE:
      return UserConsentSpecifics::PLAY_STORE;
    case consent_auditor::Feature::BACKUP_AND_RESTORE:
      return UserConsentSpecifics::BACKUP_AND_RESTORE;
    case consent_auditor::Feature::GOOGLE_LOCATION_SERVICE:
      return UserConsentSpecifics::GOOGLE_LOCATION_SERVICE;
    case consent_auditor::Feature::CHROME_UNIFIED_CONSENT:
      return UserConsentSpecifics::CHROME_UNIFIED_CONSENT;
  }
  NOTREACHED();
  return UserConsentSpecifics::FEATURE_UNSPECIFIED;
}

ConsentStatus ConvertConsentStatus(
    UserConsentTypes::ConsentStatus consent_status) {
  DCHECK_NE(consent_status,
            UserConsentTypes::ConsentStatus::
                UserConsentTypes_ConsentStatus_CONSENT_STATUS_UNSPECIFIED);

  if (consent_status ==
      UserConsentTypes::ConsentStatus::UserConsentTypes_ConsentStatus_GIVEN) {
    return ConsentStatus::GIVEN;
  }
  return ConsentStatus::NOT_GIVEN;
}

}  // namespace

ConsentAuditorImpl::ConsentAuditorImpl(
    PrefService* pref_service,
    std::unique_ptr<syncer::ConsentSyncBridge> consent_sync_bridge,
    syncer::UserEventService* user_event_service,
    const std::string& app_version,
    const std::string& app_locale,
    base::Clock* clock)
    : pref_service_(pref_service),
      consent_sync_bridge_(std::move(consent_sync_bridge)),
      user_event_service_(user_event_service),
      app_version_(app_version),
      app_locale_(app_locale),
      clock_(clock) {
  if (IsSeparateConsentTypeEnabled()) {
    DCHECK(consent_sync_bridge_ && !user_event_service_);
  } else {
    DCHECK(user_event_service_ && !consent_sync_bridge_);
  }
  DCHECK(pref_service_);
}

ConsentAuditorImpl::~ConsentAuditorImpl() {}

void ConsentAuditorImpl::Shutdown() {
  user_event_service_ = nullptr;
}

// static
void ConsentAuditorImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kLocalConsentsDictionary);
}

void ConsentAuditorImpl::RecordGaiaConsent(
    const std::string& account_id,
    Feature feature,
    const std::vector<int>& description_grd_ids,
    int confirmation_grd_id,
    ConsentStatus status) {
  DCHECK(!account_id.empty()) << "No signed-in account specified.";

  if (!base::FeatureList::IsEnabled(switches::kSyncUserConsentEvents))
    return;

  DCHECK_LE(feature, consent_auditor::Feature::FEATURE_LAST);

  switch (status) {
    case ConsentStatus::GIVEN:
      UMA_HISTOGRAM_ENUMERATION(
          "Privacy.ConsentAuditor.ConsentGiven.Feature", feature,
          static_cast<int>(consent_auditor::Feature::FEATURE_LAST) + 1);
      break;
    case ConsentStatus::NOT_GIVEN:
      UMA_HISTOGRAM_ENUMERATION(
          "Privacy.ConsentAuditor.ConsentNotGiven.Feature", feature,
          static_cast<int>(consent_auditor::Feature::FEATURE_LAST) + 1);
      break;
  }

  if (IsSeparateConsentTypeEnabled()) {
    // TODO(msramek): Pass in the actual account id.
    std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
        ConstructUserConsentSpecifics(account_id, feature, description_grd_ids,
                                      confirmation_grd_id, status);
    consent_sync_bridge_->RecordConsent(std::move(specifics));
  } else {
    // TODO(msramek): Pass in the actual account id.
    std::unique_ptr<sync_pb::UserEventSpecifics> specifics =
        ConstructUserEventSpecifics(account_id, feature, description_grd_ids,
                                    confirmation_grd_id, status);
    user_event_service_->RecordUserEvent(std::move(specifics));
  }
}

std::unique_ptr<sync_pb::UserEventSpecifics>
ConsentAuditorImpl::ConstructUserEventSpecifics(
    const std::string& account_id,
    Feature feature,
    const std::vector<int>& description_grd_ids,
    int confirmation_grd_id,
    ConsentStatus status) {
  DCHECK(!IsSeparateConsentTypeEnabled());

  auto specifics = std::make_unique<sync_pb::UserEventSpecifics>();
  specifics->set_event_time_usec(clock_->Now().since_origin().InMicroseconds());
  auto* consent = specifics->mutable_user_consent();
  consent->set_account_id(account_id);
  consent->set_feature(FeatureToUserEventProtoEnum(feature));
  for (int id : description_grd_ids) {
    consent->add_description_grd_ids(id);
  }
  consent->set_confirmation_grd_id(confirmation_grd_id);
  consent->set_locale(app_locale_);
  consent->set_status(StatusToProtoEnum(status));
  return specifics;
}

std::unique_ptr<sync_pb::UserConsentSpecifics>
ConsentAuditorImpl::ConstructUserConsentSpecifics(
    const std::string& account_id,
    Feature feature,
    const std::vector<int>& description_grd_ids,
    int confirmation_grd_id,
    ConsentStatus status) {
  DCHECK(IsSeparateConsentTypeEnabled());

  auto specifics = std::make_unique<sync_pb::UserConsentSpecifics>();
  specifics->set_client_consent_time_usec(
      clock_->Now().since_origin().InMicroseconds());
  specifics->set_account_id(account_id);
  specifics->set_feature(FeatureToUserConsentProtoEnum(feature));
  for (int id : description_grd_ids) {
    specifics->add_description_grd_ids(id);
  }
  specifics->set_confirmation_grd_id(confirmation_grd_id);
  specifics->set_locale(app_locale_);
  specifics->set_status(StatusToProtoEnum(status));
  return specifics;
}

void ConsentAuditorImpl::RecordArcPlayConsent(
    const std::string& account_id,
    const ArcPlayTermsOfServiceConsent& consent) {
  std::vector<int> description_grd_ids;
  if (consent.consent_flow() == ArcPlayTermsOfServiceConsent::SETTING_CHANGE) {
    for (int grd_id : consent.description_grd_ids()) {
      description_grd_ids.push_back(grd_id);
    }
  } else {
    description_grd_ids.push_back(consent.play_terms_of_service_text_length());

    // TODO(markusheintz): The code below is a copy from the ARC code base. This
    // will go away when the consent proto is set on the user consent specifics
    // proto.
    const std::string& hash_str = consent.play_terms_of_service_hash();
    DCHECK_EQ(base::kSHA1Length, hash_str.size());
    const uint8_t* hash = reinterpret_cast<const uint8_t*>(hash_str.data());
    for (size_t i = 0; i < base::kSHA1Length; i += 4) {
      uint32_t acc =
          hash[i] << 24 | hash[i + 1] << 16 | hash[i + 2] << 8 | hash[i + 3];
      description_grd_ids.push_back(static_cast<int>(acc));
    }
  }

  RecordGaiaConsent(account_id, Feature::PLAY_STORE, description_grd_ids,
                    consent.confirmation_grd_id(),
                    ConvertConsentStatus(consent.status()));
}

void ConsentAuditorImpl::RecordArcGoogleLocationServiceConsent(
    const std::string& account_id,
    const UserConsentTypes::ArcGoogleLocationServiceConsent& consent) {
  std::vector<int> description_grd_ids(consent.description_grd_ids().begin(),
                                       consent.description_grd_ids().end());

  RecordGaiaConsent(account_id, Feature::GOOGLE_LOCATION_SERVICE,
                    description_grd_ids, consent.confirmation_grd_id(),
                    ConvertConsentStatus(consent.status()));
}

void ConsentAuditorImpl::RecordArcBackupAndRestoreConsent(
    const std::string& account_id,
    const UserConsentTypes::ArcBackupAndRestoreConsent& consent) {
  std::vector<int> description_grd_ids(consent.description_grd_ids().begin(),
                                       consent.description_grd_ids().end());

  RecordGaiaConsent(account_id, Feature::BACKUP_AND_RESTORE,
                    description_grd_ids, consent.confirmation_grd_id(),
                    ConvertConsentStatus(consent.status()));
}

void ConsentAuditorImpl::RecordSyncConsent(
    const std::string& account_id,
    const UserConsentTypes::SyncConsent& consent) {
  std::vector<int> description_grd_ids(consent.description_grd_ids().begin(),
                                       consent.description_grd_ids().end());

  RecordGaiaConsent(account_id, Feature::CHROME_SYNC, description_grd_ids,
                    consent.confirmation_grd_id(),
                    ConvertConsentStatus(consent.status()));
}

void ConsentAuditorImpl::RecordUnifiedConsent(
    const std::string& account_id,
    const sync_pb::UserConsentTypes::UnifiedConsent& consent) {
  std::vector<int> description_grd_ids(consent.description_grd_ids().begin(),
                                       consent.description_grd_ids().end());

  RecordGaiaConsent(account_id, Feature::CHROME_UNIFIED_CONSENT,
                    description_grd_ids, consent.confirmation_grd_id(),
                    ConvertConsentStatus(consent.status()));
}

void ConsentAuditorImpl::RecordLocalConsent(
    const std::string& feature,
    const std::string& description_text,
    const std::string& confirmation_text) {
  DictionaryPrefUpdate consents_update(pref_service_,
                                       prefs::kLocalConsentsDictionary);
  base::DictionaryValue* consents = consents_update.Get();
  DCHECK(consents);

  base::DictionaryValue record;
  record.SetKey(kLocalConsentDescriptionKey, base::Value(description_text));
  record.SetKey(kLocalConsentConfirmationKey, base::Value(confirmation_text));
  record.SetKey(kLocalConsentVersionKey, base::Value(app_version_));
  record.SetKey(kLocalConsentLocaleKey, base::Value(app_locale_));

  consents->SetKey(feature, std::move(record));
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ConsentAuditorImpl::GetControllerDelegate() {
  if (consent_sync_bridge_) {
    return consent_sync_bridge_->GetControllerDelegate();
  }
  return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
}

}  // namespace consent_auditor

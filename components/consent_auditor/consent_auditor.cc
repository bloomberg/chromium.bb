// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_auditor.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/consent_auditor/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/user_events/user_event_service.h"

using UserEventSpecifics = sync_pb::UserEventSpecifics;

namespace consent_auditor {

namespace {

const char kLocalConsentDescriptionKey[] = "description";
const char kLocalConsentConfirmationKey[] = "confirmation";
const char kLocalConsentVersionKey[] = "version";
const char kLocalConsentLocaleKey[] = "locale";

UserEventSpecifics::UserConsent::ConsentStatus ToProtoEnum(
    consent_auditor::ConsentStatus status) {
  switch (status) {
    case consent_auditor::ConsentStatus::REVOKED:
      return UserEventSpecifics::UserConsent::REVOKED;
    case consent_auditor::ConsentStatus::GIVEN:
      return UserEventSpecifics::UserConsent::GIVEN;
  }
  NOTREACHED();
  return UserEventSpecifics::UserConsent::UNSPECIFIED;
}

}  // namespace

ConsentAuditor::ConsentAuditor(PrefService* pref_service,
                               syncer::UserEventService* user_event_service,
                               const std::string& app_version,
                               const std::string& app_locale)
    : pref_service_(pref_service),
      user_event_service_(user_event_service),
      app_version_(app_version),
      app_locale_(app_locale) {
  DCHECK(pref_service_);
  DCHECK(user_event_service_);
}

ConsentAuditor::~ConsentAuditor() {}

void ConsentAuditor::Shutdown() {
  user_event_service_ = nullptr;
}

// static
void ConsentAuditor::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kLocalConsentsDictionary);
}

void ConsentAuditor::RecordGaiaConsent(
    const std::string& feature,
    const std::vector<int>& consent_grd_ids,
    const std::vector<std::string>& placeholder_replacements,
    ConsentStatus status) {
  if (!base::FeatureList::IsEnabled(switches::kSyncUserConsentEvents))
    return;
  std::unique_ptr<sync_pb::UserEventSpecifics> specifics = ConstructUserConsent(
      feature, consent_grd_ids, placeholder_replacements, status);
  // For real usage, UserEventSyncBridge should always be ready to receive
  // events when a consent gets recorded.
  // FakeUserEventService doesn't have a sync bridge.
  // TODO(crbug.com/709094, crbug.com/761485): Remove this check when the store
  // initializes synchronously and is instantly ready to receive data.
  DCHECK(!user_event_service_->GetSyncBridge() ||
         user_event_service_->GetSyncBridge()
             ->change_processor()
             ->IsTrackingMetadata());
  user_event_service_->RecordUserEvent(std::move(specifics));
}

std::unique_ptr<sync_pb::UserEventSpecifics>
ConsentAuditor::ConstructUserConsent(
    const std::string& feature,
    const std::vector<int>& consent_grd_ids,
    const std::vector<std::string>& placeholder_replacements,
    ConsentStatus status) {
  auto specifics = base::MakeUnique<sync_pb::UserEventSpecifics>();
  specifics->set_event_time_usec(
      base::Time::Now().since_origin().InMicroseconds());
  auto* consent = specifics->mutable_user_consent();
  consent->set_feature(feature);
  for (int id : consent_grd_ids) {
    consent->add_consent_grd_ids(id);
  }
  for (const auto& string : placeholder_replacements) {
    consent->add_placeholder_replacements(string);
  }
  consent->set_locale(app_locale_);
  consent->set_status(ToProtoEnum(status));
  return specifics;
}

void ConsentAuditor::RecordLocalConsent(const std::string& feature,
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

}  // namespace consent_auditor

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
#include "components/sync/user_events/user_event_service.h"

namespace consent_auditor {

namespace {

const char kLocalConsentDescriptionKey[] = "description";
const char kLocalConsentConfirmationKey[] = "confirmation";
const char kLocalConsentVersionKey[] = "version";
const char kLocalConsentLocaleKey[] = "locale";

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
  // TODO(crbug.com/781765): Implement consent recording.
  NOTIMPLEMENTED();
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

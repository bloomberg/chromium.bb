// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/state_store.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/common/pref_names.h"

namespace safe_browsing {

// StateStore::Transaction -----------------------------------------------------

StateStore::Transaction::Transaction(StateStore* store) : store_(store) {
#if DCHECK_IS_ON()
  DCHECK(!store_->has_transaction_);
  store_->has_transaction_ = true;
#endif
}

StateStore::Transaction::~Transaction() {
#if DCHECK_IS_ON()
  store_->has_transaction_ = false;
#endif
}

void StateStore::Transaction::MarkAsReported(IncidentType type,
                                             const std::string& key,
                                             IncidentDigest digest) {
  std::string type_string(base::IntToString(static_cast<int32_t>(type)));
  base::DictionaryValue* incidents_sent = GetPrefDict();
  base::DictionaryValue* type_dict = nullptr;
  if (!incidents_sent->GetDictionaryWithoutPathExpansion(type_string,
                                                         &type_dict)) {
    type_dict = new base::DictionaryValue();
    incidents_sent->SetWithoutPathExpansion(type_string, type_dict);
  }
  type_dict->SetStringWithoutPathExpansion(key, base::UintToString(digest));
}

void StateStore::Transaction::ClearForType(IncidentType type) {
  // Nothing to do if the pref dict does not exist.
  if (!store_->incidents_sent_)
    return;

  // Use the read-only view on the preference to figure out if there is a value
  // to remove before committing to making a change since any use of GetPrefDict
  // will result in a full serialize-and-write operation on the preferences
  // store.
  std::string type_string(base::IntToString(static_cast<int32_t>(type)));
  const base::DictionaryValue* type_dict = nullptr;
  if (store_->incidents_sent_->GetDictionaryWithoutPathExpansion(type_string,
                                                                 &type_dict)) {
    GetPrefDict()->RemoveWithoutPathExpansion(type_string, nullptr);
  }
}

base::DictionaryValue* StateStore::Transaction::GetPrefDict() {
  if (!pref_update_) {
    pref_update_.reset(new DictionaryPrefUpdate(
        store_->profile_->GetPrefs(), prefs::kSafeBrowsingIncidentsSent));
    // Getting the dict will cause it to be created if it doesn't exist.
    // Unconditionally refresh the store's read-only view on the preference so
    // that it will always be correct.
    store_->incidents_sent_ = pref_update_->Get();
  }
  return pref_update_->Get();
}


// StateStore ------------------------------------------------------------------

StateStore::StateStore(Profile* profile)
    : profile_(profile),
      incidents_sent_(nullptr)
#if DCHECK_IS_ON()
      ,
      has_transaction_(false)
#endif
{
  // Cache a read-only view of the preference.
  const base::Value* value =
      profile_->GetPrefs()->GetUserPrefValue(prefs::kSafeBrowsingIncidentsSent);
  if (value)
    value->GetAsDictionary(&incidents_sent_);

  Transaction transaction(this);
  CleanLegacyValues(&transaction);
}

StateStore::~StateStore() {
#if DCHECK_IS_ON()
  DCHECK(!has_transaction_);
#endif
}

bool StateStore::HasBeenReported(IncidentType type,
                                 const std::string& key,
                                 IncidentDigest digest) {
  const base::DictionaryValue* type_dict = nullptr;
  std::string digest_string;
  return (incidents_sent_ &&
          incidents_sent_->GetDictionaryWithoutPathExpansion(
              base::IntToString(static_cast<int32_t>(type)), &type_dict) &&
          type_dict->GetStringWithoutPathExpansion(key, &digest_string) &&
          digest_string == base::UintToString(digest));
}

void StateStore::CleanLegacyValues(Transaction* transaction) {
  static const IncidentType kLegacyTypes[] = {
      // TODO(grt): remove in M44 (crbug.com/451173).
      IncidentType::OMNIBOX_INTERACTION,
  };

  for (IncidentType type : kLegacyTypes)
    transaction->ClearForType(type);
}

}  // namespace safe_browsing

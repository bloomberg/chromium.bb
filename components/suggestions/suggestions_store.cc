// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_store.h"

#include <string>

#include "base/base64.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/suggestions/suggestions_pref_names.h"

namespace suggestions {

SuggestionsStore::SuggestionsStore(PrefService* profile_prefs)
    : pref_service_(profile_prefs) {
  DCHECK(profile_prefs);
}

SuggestionsStore::~SuggestionsStore() {}

bool SuggestionsStore::LoadSuggestions(SuggestionsProfile* suggestions) {
  DCHECK(suggestions);

  const std::string base64_suggestions_data =
      pref_service_->GetString(prefs::kSuggestionsData);
  if (base64_suggestions_data.empty()) {
    suggestions->Clear();
    return false;
  }

  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string suggestions_data;
  if (!base::Base64Decode(base64_suggestions_data, &suggestions_data) ||
      !suggestions->ParseFromString(suggestions_data)) {
    VLOG(1) << "Suggestions data in profile pref is corrupt, clearing it.";
    suggestions->Clear();
    ClearSuggestions();
    return false;
  }

  // Filter expired suggestions and update the stored suggestions if at least
  // one was filtered. Return false if all suggestions are filtered.
  int unfiltered_size = suggestions->suggestions_size();
  FilterExpiredSuggestions(suggestions);
  if (suggestions->suggestions_size() != unfiltered_size) {
    if (!suggestions->suggestions_size()) {
      suggestions->Clear();
      ClearSuggestions();
      return false;
    } else {
      StoreSuggestions(*suggestions);
    }
  }

  return true;
}

void SuggestionsStore::FilterExpiredSuggestions(
    SuggestionsProfile* suggestions) {
  SuggestionsProfile filtered_suggestions;
  int64 now_usec = (base::Time::NowFromSystemTime() - base::Time::UnixEpoch())
      .ToInternalValue();

  for (int i = 0; i < suggestions->suggestions_size(); ++i) {
    ChromeSuggestion* suggestion = suggestions->mutable_suggestions(i);
    if (!suggestion->has_expiry_ts() || suggestion->expiry_ts() > now_usec) {
      filtered_suggestions.add_suggestions()->Swap(suggestion);
    }
  }
  suggestions->Swap(&filtered_suggestions);
}

bool SuggestionsStore::StoreSuggestions(const SuggestionsProfile& suggestions) {
  std::string suggestions_data;
  if (!suggestions.SerializeToString(&suggestions_data)) return false;

  std::string base64_suggestions_data;
  base::Base64Encode(suggestions_data, &base64_suggestions_data);

  pref_service_->SetString(prefs::kSuggestionsData, base64_suggestions_data);
  return true;
}

void SuggestionsStore::ClearSuggestions() {
  pref_service_->ClearPref(prefs::kSuggestionsData);
}

// static
void SuggestionsStore::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kSuggestionsData, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace suggestions

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/blacklist_store.h"

#include <set>
#include <string>

#include "base/base64.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace suggestions {

namespace {

void PopulateBlacklistSet(const SuggestionsBlacklist& blacklist_proto,
                          std::set<std::string>* blacklist_set) {
  blacklist_set->clear();
  for (int i = 0; i < blacklist_proto.urls_size(); ++i) {
    blacklist_set->insert(blacklist_proto.urls(i));
  }
}

void PopulateBlacklistProto(const std::set<std::string>& blacklist_set,
                            SuggestionsBlacklist* blacklist_proto) {
  blacklist_proto->Clear();
  for (std::set<std::string>::const_iterator it = blacklist_set.begin();
       it != blacklist_set.end(); ++it) {
    blacklist_proto->add_urls(*it);
  }
}

}  // namespace

BlacklistStore::BlacklistStore(PrefService* profile_prefs)
    : pref_service_(profile_prefs) {
  DCHECK(pref_service_);

  // Log the blacklist's size. A single BlacklistStore is created for the
  // SuggestionsService; this will run once.
  SuggestionsBlacklist blacklist_proto;
  LoadBlacklist(&blacklist_proto);
  UMA_HISTOGRAM_COUNTS_10000("Suggestions.LocalBlacklistSize",
                             blacklist_proto.urls_size());
}

BlacklistStore::~BlacklistStore() {}

bool BlacklistStore::BlacklistUrl(const GURL& url) {
  if (!url.is_valid()) return false;

  SuggestionsBlacklist blacklist_proto;
  LoadBlacklist(&blacklist_proto);

  std::set<std::string> blacklist_set;
  PopulateBlacklistSet(blacklist_proto, &blacklist_set);

  if (!blacklist_set.insert(url.spec()).second) {
    // |url| was already in the blacklist.
    return true;
  }

  PopulateBlacklistProto(blacklist_set, &blacklist_proto);
  return StoreBlacklist(blacklist_proto);
}

bool BlacklistStore::GetFirstUrlFromBlacklist(GURL* url) {
  SuggestionsBlacklist blacklist;
  LoadBlacklist(&blacklist);
  if (!blacklist.urls_size()) return false;
  GURL blacklisted(blacklist.urls(0));
  url->Swap(&blacklisted);
  return true;
}

bool BlacklistStore::RemoveUrl(const GURL& url) {
  if (!url.is_valid()) return false;
  const std::string removal_candidate = url.spec();

  SuggestionsBlacklist blacklist;
  LoadBlacklist(&blacklist);

  SuggestionsBlacklist updated_blacklist;
  for (int i = 0; i < blacklist.urls_size(); ++i) {
    if (blacklist.urls(i) != removal_candidate)
      updated_blacklist.add_urls(blacklist.urls(i));
  }

  return StoreBlacklist(updated_blacklist);
}

void BlacklistStore::FilterSuggestions(SuggestionsProfile* profile) {
  if (!profile->suggestions_size())
    return;  // Empty profile, nothing to filter.

  SuggestionsBlacklist blacklist_proto;
  if (!LoadBlacklist(&blacklist_proto)) {
    // There was an error loading the blacklist. The blacklist was cleared and
    // there's nothing to be done about it.
    return;
  }
  if (!blacklist_proto.urls_size())
    return;  // Empty blacklist, nothing to filter.

  std::set<std::string> blacklist_set;
  PopulateBlacklistSet(blacklist_proto, &blacklist_set);

  // Populate the filtered suggestions.
  SuggestionsProfile filtered_profile;
  for (int i = 0; i < profile->suggestions_size(); ++i) {
    if (blacklist_set.find(profile->suggestions(i).url()) ==
        blacklist_set.end()) {
      // This suggestion is not blacklisted.
      ChromeSuggestion* suggestion = filtered_profile.add_suggestions();
      // Note: swapping!
      suggestion->Swap(profile->mutable_suggestions(i));
    }
  }

  // Swap |profile| and |filtered_profile|.
  profile->Swap(&filtered_profile);
}

// static
void BlacklistStore::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kSuggestionsBlacklist, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool BlacklistStore::LoadBlacklist(SuggestionsBlacklist* blacklist) {
  DCHECK(blacklist);

  const std::string base64_blacklist_data =
      pref_service_->GetString(prefs::kSuggestionsBlacklist);
  if (base64_blacklist_data.empty()) {
    blacklist->Clear();
    return false;
  }

  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string blacklist_data;
  if (!base::Base64Decode(base64_blacklist_data, &blacklist_data) ||
      !blacklist->ParseFromString(blacklist_data)) {
    VLOG(1) << "Suggestions blacklist data in profile pref is corrupt, "
            << " clearing it.";
    blacklist->Clear();
    ClearBlacklist();
    return false;
  }

  return true;
}

bool BlacklistStore::StoreBlacklist(const SuggestionsBlacklist& blacklist) {
  std::string blacklist_data;
  if (!blacklist.SerializeToString(&blacklist_data)) return false;

  std::string base64_blacklist_data;
  base::Base64Encode(blacklist_data, &base64_blacklist_data);

  pref_service_->SetString(prefs::kSuggestionsBlacklist, base64_blacklist_data);
  return true;
}

void BlacklistStore::ClearBlacklist() {
  pref_service_->ClearPref(prefs::kSuggestionsBlacklist);
}

}  // namespace suggestions

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "net/http/http_util.h"

namespace {

const char* const cookie_headers[2] = { "cookie", "set-cookie" };

}  // namespace

const unsigned int Blacklist::kBlockAll = 1;
const unsigned int Blacklist::kBlockCookies = 1 << 1;
const unsigned int Blacklist::kDontSendReferrer = 1 << 2;
const unsigned int Blacklist::kDontSendUserAgent = 1 << 3;
const unsigned int Blacklist::kBlockUnsecure = 1 << 4;
const unsigned int Blacklist::kBlockRequest = kBlockAll | kBlockUnsecure;
const unsigned int Blacklist::kModifySentHeaders =
    kBlockCookies | kDontSendUserAgent | kDontSendReferrer;
const unsigned int Blacklist::kModifyReceivedHeaders = kBlockCookies;

// static
bool Blacklist::Matches(const std::string& pattern, const std::string& url) {
  if (pattern.size() > url.size())
    return false;

  std::string::size_type p = 0;
  std::string::size_type u = 0;

  while (pattern[p] != '\0' && url[u] != '\0') {
    if (pattern[p] == '@') {
      while (pattern[++p] == '@');  // Consecutive @ are redundant.

      if (pattern[p] == '\0')
        return true;  // Nothing to match after the @.

      // Look for another wildcard to determine pattern-chunk.
      std::string::size_type tp = pattern.find_first_of("@", p);

      // If it must match until the end, compare the last characters.
      if (tp == std::string::npos) {
        std::string::size_type ur = url.size() - u;
        std::string::size_type pr = pattern.size() - p;
        return (pr <= ur) &&
            (url.compare(url.size() - pr, pr, pattern.c_str() + p) == 0);
      }

      // Else find the pattern chunk which is pattern[p:tp]
      std::string::size_type tu = url.find(pattern.c_str() + p, u, tp - p);
      if (tu == std::string::npos)
        return false;  // Pattern chunk not found.

      // Since tp is strictly greater than p, both u and p always increase.
      u = tu + tp - p;
      p = tp;
      continue;
    }

    // Match non-wildcard character.
    if (pattern[p++] != url[u++])
      return false;
  }
  return pattern[p] == '\0';
}

bool Blacklist::Entry::IsBlocked(const GURL& url) const {
  return (attributes_ & kBlockAll) ||
    ((attributes_ & kBlockUnsecure) && !url.SchemeIsSecure());
}

Blacklist::Entry::Entry(const std::string& pattern, const Provider* provider,
                        bool is_exception)
    : attributes_(0),
      is_exception_(is_exception),
      pattern_(pattern),
      provider_(provider) {}

void Blacklist::Entry::AddAttributes(unsigned int attributes) {
  attributes_ |= attributes;
}

bool Blacklist::Match::IsBlocked(const GURL& url) const {
  return (attributes() & kBlockAll) ||
    ((attributes() & kBlockUnsecure) && !url.SchemeIsSecure());
}

Blacklist::Match::Match() : matching_attributes_(0), exception_attributes_(0) {}

void Blacklist::Match::AddEntry(const Entry* entry) {
  if (entry->is_exception()) {
    exception_attributes_ |= entry->attributes();
    exception_entries_.push_back(entry);
  } else {
    matching_attributes_ |= entry->attributes();
    matching_entries_.push_back(entry);
  }
}

Blacklist::Blacklist(PrefService* prefs) : prefs_(prefs) {
  LoadPreferences();
}

Blacklist::~Blacklist() {
}

void Blacklist::AddEntry(Entry* entry) {
  DCHECK(entry);
  blacklist_.push_back(linked_ptr<Entry>(entry));
}

void Blacklist::AddProvider(Provider* provider) {
  DCHECK(provider);
  providers_.push_back(linked_ptr<Provider>(provider));
}

// Returns a pointer to the Blacklist-owned entry which matches the given
// URL. If no matching Entry is found, returns null.
Blacklist::Match* Blacklist::FindMatch(const GURL& url) const {
  // Never match something which is not http, https or ftp.
  // TODO(idanan): Investigate if this would be an inclusion test instead of an
  // exclusion test and if there are other schemes to test for.
  if (!url.SchemeIs(chrome::kHttpScheme) &&
      !url.SchemeIs(chrome::kHttpsScheme) &&
      !url.SchemeIs(chrome::kFtpScheme))
    return 0;
  std::string url_spec = GetURLAsLookupString(url);
  Match* match = NULL;
  for (EntryList::const_iterator i = blacklist_.begin();
       i != blacklist_.end(); ++i) {
    if (Matches((*i)->pattern(), url_spec)) {
      if (!match)
        match = new Match;
      match->AddEntry(i->get());
    }
  }
  return match;
}

bool Blacklist::LoadEntryPreference(const ListValue& pref,
                                    const Provider* provider) {
  EntryList entries;
  for (ListValue::const_iterator i = pref.begin(); i != pref.end(); ++i) {
    if (!(*i)->IsType(Value::TYPE_DICTIONARY))
      return false;
    const DictionaryValue* entry_pref = static_cast<DictionaryValue*>(*i);
    std::string pattern;
    int attributes;
    bool is_exception;
    if (!(entry_pref->GetString(L"pattern", &pattern) &&
          entry_pref->GetInteger(L"attributes", &attributes) &&
          entry_pref->GetBoolean(L"exception", &is_exception)))
      return false;
    Entry* entry = new Entry(pattern, provider, is_exception);
    entry->AddAttributes(static_cast<unsigned int>(attributes));
    entries.push_back(linked_ptr<Entry>(entry));
  }
  blacklist_.insert(blacklist_.end(), entries.begin(), entries.end());
  return true;
}

bool Blacklist::LoadProviderPreference(const DictionaryValue& pref,
                                       const std::wstring& path) {
  std::string name, url;
  ListValue* entries;
  if (!(pref.GetString(L"name", &name) && pref.GetString(L"url", &url) &&
        pref.GetList(L"entries", &entries)))
    return false;
  linked_ptr<Provider> provider(new Provider(name, url, path));
  if (LoadEntryPreference(*entries, provider.get()))
    providers_.push_back(provider);
    return true;
  return false;
}

bool Blacklist::LoadPreferences() {
  DCHECK(prefs_);
  const DictionaryValue* blacklist_rules = prefs_->GetDictionary(
      prefs::kPrivacyFilterRules);
  if (!blacklist_rules)
    return false;
  bool result = true;
  for (DictionaryValue::key_iterator key = blacklist_rules->begin_keys();
       key != blacklist_rules->end_keys(); ++key) {
    DictionaryValue* provider;
    if (!(blacklist_rules->GetDictionaryWithoutPathExpansion(*key, &provider) &&
         LoadProviderPreference(*provider, *key)))
      result = false;
  }
  return result;
}

// static
std::string Blacklist::GetURLAsLookupString(const GURL& url) {
  std::string url_spec = url.host() + url.path();
  if (!url.query().empty())
    url_spec = url_spec + "?" + url.query();

  return url_spec;
}

// static
void Blacklist::RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterDictionaryPref(prefs::kPrivacyFilterRules);
}

// static
std::string Blacklist::StripCookies(const std::string& header) {
  return net::HttpUtil::StripHeaders(header, cookie_headers, 2);
}

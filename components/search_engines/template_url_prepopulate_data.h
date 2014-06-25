// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "components/search_engines/search_engine_type.h"

class GURL;
class PrefService;
class Profile;
class SearchTermsData;
class TemplateURL;
struct TemplateURLData;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace TemplateURLPrepopulateData {

extern const int kMaxPrepopulatedEngineID;

#if defined(OS_ANDROID)
// This must be called early only once. |country_code| is the country code at
// install following the ISO-3166 specification.
void InitCountryCode(const std::string& country_code);
#endif

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns the current version of the prepopulate data, so callers can know when
// they need to re-merge. If the prepopulate data comes from the preferences
// file then it returns the version specified there.
int GetDataVersion(PrefService* prefs);

// Loads the set of TemplateURLs from the prepopulate data. On return,
// |default_search_provider_index| is set to the index of the default search
// provider.
ScopedVector<TemplateURLData> GetPrepopulatedEngines(
    PrefService* prefs,
    size_t* default_search_provider_index);

// Removes prepopulated engines and their version stored in user prefs.
void ClearPrepopulatedEnginesInPrefs(PrefService* prefs);

// Returns the default search provider specified by the prepopulate data, which
// may be NULL.
// If |prefs| is NULL, any search provider overrides from the preferences are
// not used.
scoped_ptr<TemplateURLData> GetPrepopulatedDefaultSearch(PrefService* prefs);

// Returns the type of the provided engine, or SEARCH_ENGINE_OTHER if no engines
// match.  This checks the TLD+1 for the most part, but will report the type as
// SEARCH_ENGINE_GOOGLE for any hostname that causes
// google_util::IsGoogleHostname() to return true.
//
// NOTE: Must be called on the UI thread.
SearchEngineType GetEngineType(const TemplateURL& template_url,
                               const SearchTermsData& search_terms_data);

// Like the above, but takes a GURL which is expected to represent a search URL.
// This may be called on any thread.
SearchEngineType GetEngineType(const GURL& url);

}  // namespace TemplateURLPrepopulateData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

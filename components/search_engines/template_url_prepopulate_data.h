// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "components/search_engines/search_engine_type.h"

class GURL;
class PrefService;
class SearchTermsData;
class TemplateURL;
struct TemplateURLData;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace TemplateURLPrepopulateData {

struct PrepopulatedEngine;

extern const int kMaxPrepopulatedEngineID;

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

// Returns a TemplateURLData for the specified prepopulated engine.
std::unique_ptr<TemplateURLData> MakeTemplateURLDataFromPrepopulatedEngine(
    const PrepopulatedEngine& engine);

// Removes prepopulated engines and their version stored in user prefs.
void ClearPrepopulatedEnginesInPrefs(PrefService* prefs);

// Returns the default search provider specified by the prepopulate data, which
// may be NULL.
// If |prefs| is NULL, any search provider overrides from the preferences are
// not used.
std::unique_ptr<TemplateURLData> GetPrepopulatedDefaultSearch(
    PrefService* prefs);

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

// Returns the identifier for the user current country. Used to update the list
// of search engines when user switches device region settings. For use on iOS
// only.
// TODO(ios): Once user can customize search engines ( http://crbug.com/153047 )
// this declaration should be removed and the definition in the .cc file be
// moved back to the anonymous namespace.
int GetCurrentCountryID();

}  // namespace TemplateURLPrepopulateData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

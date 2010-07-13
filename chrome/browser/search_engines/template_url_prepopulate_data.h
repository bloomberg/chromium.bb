// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

#include <vector>

class PrefService;
class TemplateURL;

namespace TemplateURLPrepopulateData {

// Enum to record the user's default search engine choice in UMA:
enum SearchEngineType {
    SEARCH_ENGINE_OTHER = 0,  // At the top in case of future list changes.
    SEARCH_ENGINE_GOOGLE,
    SEARCH_ENGINE_YAHOO,
    SEARCH_ENGINE_YAHOOJP,
    SEARCH_ENGINE_BING,
    SEARCH_ENGINE_ASK,
    SEARCH_ENGINE_YANDEX,
    SEARCH_ENGINE_SEZNAM,
    SEARCH_ENGINE_CENTRUM,
    SEARCH_ENGINE_NETSPRINT,
    SEARCH_ENGINE_VIRGILIO,
    SEARCH_ENGINE_MAILRU,
    SEARCH_ENGINE_MAX  // Bounding max value needed for UMA histogram macro.
};

void RegisterUserPrefs(PrefService* prefs);

// Returns the current version of the prepopulate data, so callers can know when
// they need to re-merge. If the prepopulate data comes from the preferences
// file then it returns the version specified there.
int GetDataVersion(PrefService* prefs);

// Loads the set of TemplateURLs from the prepopulate data.  Ownership of the
// TemplateURLs is passed to the caller.  On return,
// |default_search_provider_index| is set to the index of the default search
// provider.
void GetPrepopulatedEngines(PrefService* prefs,
                            std::vector<TemplateURL*>* t_urls,
                            size_t* default_search_provider_index);

// Returns the type of the search engine to be recorded in UMA. The type
// is determined by mapping the search engine's |id| to the set of search
// engines we are interested in. Because this is only a temporary test
// for a small set of search engines, we use this simple switch statement
// instead of embedding a UMA type as part of the struct of every search
// engine.
SearchEngineType GetSearchEngineType(const TemplateURL* search_engine);

}  // namespace TemplateURLPrepopulateData

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#pragma once

#include <vector>

class GURL;
class PrefService;
class TemplateURL;

namespace TemplateURLPrepopulateData {

// Enum to record the user's default search engine choice in UMA.  Add new
// search engines at the bottom and do not delete from this list, so as not
// to disrupt UMA data already recorded.
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
    SEARCH_ENGINE_ABCSOK,
    SEARCH_ENGINE_ALTAVISTA,
    SEARCH_ENGINE_BAIDU,
    SEARCH_ENGINE_DAUM,
    SEARCH_ENGINE_DELFI,
    SEARCH_ENGINE_DIRI,
    SEARCH_ENGINE_GOO,
    SEARCH_ENGINE_IN,
    SEARCH_ENGINE_NAJDI,
    SEARCH_ENGINE_NAVER,
    SEARCH_ENGINE_NETI,
    SEARCH_ENGINE_OK,
    SEARCH_ENGINE_POGODAK,
    SEARCH_ENGINE_POGODOK_MK,
    SEARCH_ENGINE_RAMBLER,
    SEARCH_ENGINE_SANOOK,
    SEARCH_ENGINE_SAPO,
    SEARCH_ENGINE_TUT,
    SEARCH_ENGINE_WALLA,
    SEARCH_ENGINE_ZOZNAM,
    SEARCH_ENGINE_YAHOOQC,
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

// Returns the default search provider specified by the prepopulate data.
// The caller owns the returned value, which may be NULL.
TemplateURL* GetPrepopulatedDefaultSearch(PrefService* prefs);

// Returns a TemplateURL from the prepopulated data which has the same origin
// as the given url.  The caller is responsible for deleting the returned
// TemplateURL.
TemplateURL* GetEngineForOrigin(PrefService* prefs, const GURL& url_to_find);

}  // namespace TemplateURLPrepopulateData

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

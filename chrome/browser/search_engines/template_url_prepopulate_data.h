// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#pragma once

#include <stddef.h>
#include <string>
#include <vector>

class GURL;
class PrefService;
class TemplateURL;

namespace TemplateURLPrepopulateData {

extern const int kMaxPrepopulatedEngineID;

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
// If |prefs| is NULL, search provider overrides from preferences are not used.
TemplateURL* GetPrepopulatedDefaultSearch(PrefService* prefs);

// Returns a TemplateURL from the prepopulated data which has the same origin
// as the given url.  The caller is responsible for deleting the returned
// TemplateURL.
TemplateURL* GetEngineForOrigin(PrefService* prefs, const GURL& url_to_find);

// Returns search engine logo for URLs known to have a search engine logo.
int GetSearchEngineLogo(const GURL& url_to_find);

// Returns the prepopulated search provider whose search URL matches
// |search_url| or NULL if none is found.  The caller is responsible for
// deleting the returned TemplateURL.
TemplateURL* FindPrepopulatedEngine(const std::string& search_url);

}  // namespace TemplateURLPrepopulateData

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/search_engines/search_engine_type.h"

class GURL;
class PrefService;
class Profile;
class TemplateURL;

namespace TemplateURLPrepopulateData {

extern const int kMaxPrepopulatedEngineID;

// Sizes at which search provider logos are available.
enum LogoSize {
  LOGO_100_PERCENT,
  LOGO_200_PERCENT,
};

#if defined(OS_ANDROID)

// This must be called early only once. |country_code| is the country code at
// install following the ISO-3166 specification.
void InitCountryCode(const std::string& country_code);

#endif

void RegisterUserPrefs(PrefService* prefs);

// Returns the current version of the prepopulate data, so callers can know when
// they need to re-merge. If the prepopulate data comes from the preferences
// file then it returns the version specified there.
int GetDataVersion(PrefService* prefs);

// Loads the set of TemplateURLs from the prepopulate data.  Ownership of the
// TemplateURLs is passed to the caller.  On return,
// |default_search_provider_index| is set to the index of the default search
// provider.
void GetPrepopulatedEngines(Profile* profile,
                            std::vector<TemplateURL*>* t_urls,
                            size_t* default_search_provider_index);

// Returns the default search provider specified by the prepopulate data.
// The caller owns the returned value, which may be NULL.
// If |profile| is NULL, any search provider overrides from the preferences are
// not used.
TemplateURL* GetPrepopulatedDefaultSearch(Profile* profile);

// Returns the type of the matching engine, or SEARCH_ENGINE_OTHER if no engines
// match.  This uses same-origin checks unless the |url| is a Google seach URL,
// in which case we'll identify any valid Google hostname as "Google".
//
// NOTE: Must be called on the UI thread.
SearchEngineType GetEngineType(const std::string& url);

// Returns the logo at the specified |size| for |template_url|.  If no logo is
// known, this will return an empty GURL.
//
// NOTE: Must be called on the UI thread.
GURL GetLogoURL(const TemplateURL& template_url, LogoSize size);

}  // namespace TemplateURLPrepopulateData

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

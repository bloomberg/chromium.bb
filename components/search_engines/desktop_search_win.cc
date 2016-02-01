// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/desktop_search_win.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "net/base/url_util.h"

namespace prefs {
const char kWindowsDesktopSearchRedirectionPref[] =
    "windows_desktop_search_redirection";
}  // namespace prefs

const base::Feature kWindowsDesktopSearchRedirectionFeature = {
    "WindowsDesktopSearchRedirection", base::FEATURE_DISABLED_BY_DEFAULT
};

void RegisterWindowsDesktopSearchRedirectionPref(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kWindowsDesktopSearchRedirectionPref,
                                false);
}

bool ShouldRedirectWindowsDesktopSearchToDefaultSearchEngine(
    PrefService* pref_service) {
  DCHECK(pref_service);
  return base::FeatureList::IsEnabled(
             kWindowsDesktopSearchRedirectionFeature) &&
         pref_service->GetBoolean(prefs::kWindowsDesktopSearchRedirectionPref);
}

bool DetectWindowsDesktopSearch(const GURL& url,
                                const SearchTermsData& search_terms_data,
                                base::string16* search_terms) {
  DCHECK(search_terms);

  scoped_ptr<TemplateURLData> template_url_data =
      TemplateURLPrepopulateData::MakeTemplateURLDataFromPrepopulatedEngine(
          TemplateURLPrepopulateData::bing);
  TemplateURL template_url(*template_url_data);

  if (!template_url.ExtractSearchTermsFromURL(url, search_terms_data,
                                              search_terms))
    return false;

  // Query parameter that tells the source of a Bing search URL, and values
  // associated with Windows desktop search.
  const char kBingSourceQueryKey[] = "form";
  const char kBingSourceDesktopText[] = "WNSGPH";
  const char kBingSourceDesktopVoice[] = "WNSBOX";

  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    // Use a case-insensitive comparison because the key is sometimes in capital
    // letters.
    if (base::EqualsCaseInsensitiveASCII(it.GetKey(), kBingSourceQueryKey)) {
      const std::string source = it.GetValue();
      if (source == kBingSourceDesktopText || source == kBingSourceDesktopVoice)
        return true;
    }
  }

  search_terms->clear();
  return false;
}

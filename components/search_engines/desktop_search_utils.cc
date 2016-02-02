// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/desktop_search_utils.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "net/base/url_util.h"

namespace {

// Values for the Search.DesktopSearch.URLAction histogram.
enum DesktopSearchURLAction {
  DESKTOP_SEARCH_URL_ACTION_NO_REDIRECTION_FEATURE_DISABLED = 0,
  DESKTOP_SEARCH_URL_ACTION_NO_REDIRECTION_DEFAULT_SEARCH_IS_BING = 1,
  DESKTOP_SEARCH_URL_ACTION_NO_REDIRECTION_INVALID_SEARCH_ENGINE = 2,
  DESKTOP_SEARCH_URL_ACTION_REDIRECTION = 3,
  DESKTOP_SEARCH_URL_ACTION_MAX
};

void RecordDesktopSearchURLAction(DesktopSearchURLAction action) {
  DCHECK_LT(action, DESKTOP_SEARCH_URL_ACTION_MAX);
  UMA_HISTOGRAM_ENUMERATION("Search.DesktopSearch.URLAction", action,
                            DESKTOP_SEARCH_URL_ACTION_MAX);
}

// Detects whether a |url| comes from a desktop search. If so, puts the search
// terms in |search_terms| and returns true.
bool DetectDesktopSearch(const GURL& url,
                         const SearchTermsData& search_terms_data,
                         base::string16* search_terms) {
  DCHECK(search_terms);
  search_terms->clear();

  scoped_ptr<TemplateURLData> template_url_data =
      TemplateURLPrepopulateData::MakeTemplateURLDataFromPrepopulatedEngine(
          TemplateURLPrepopulateData::bing);
  TemplateURL template_url(*template_url_data);
  if (!template_url.ExtractSearchTermsFromURL(url, search_terms_data,
                                              search_terms))
    return false;

  // Query parameter that tells the source of a Bing search URL, and values
  // associated with desktop search.
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

  return false;
}

}  // namespace

namespace prefs {
const char kDesktopSearchRedirectionInfobarShownPref[] =
    "desktop_search_redirection_infobar_shown";
}  // namespace prefs

const base::Feature kDesktopSearchRedirectionFeature{
    "DesktopSearchRedirection", base::FEATURE_DISABLED_BY_DEFAULT};

void RegisterDesktopSearchRedirectionPref(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kDesktopSearchRedirectionInfobarShownPref, false);
}

bool ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded(
    const PrefService* pref_service,
    TemplateURLService* template_url_service,
    GURL* url) {
  DCHECK(pref_service);
  DCHECK(template_url_service);
  DCHECK(url);

  // Check if |url| is a desktop search.
  base::string16 search_terms;
  if (!DetectDesktopSearch(*url, template_url_service->search_terms_data(),
                           &search_terms))
    return false;

  // Record that the user searched from the desktop.
  base::RecordAction(base::UserMetricsAction("DesktopSearch"));

  // Check if the redirection feature is enabled.
  if (!base::FeatureList::IsEnabled(kDesktopSearchRedirectionFeature)) {
    RecordDesktopSearchURLAction(
        DESKTOP_SEARCH_URL_ACTION_NO_REDIRECTION_FEATURE_DISABLED);
    return false;
  }

  // Check if the default search engine is Bing.
  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();
  if (default_search_engine &&
      TemplateURLPrepopulateData::GetEngineType(
          *default_search_engine, template_url_service->search_terms_data()) ==
          SEARCH_ENGINE_BING) {
    RecordDesktopSearchURLAction(
        DESKTOP_SEARCH_URL_ACTION_NO_REDIRECTION_DEFAULT_SEARCH_IS_BING);
    return false;
  }

  // Replace |url| by a default search engine URL.
  GURL search_url(
      GetDefaultSearchURLForSearchTerms(template_url_service, search_terms));
  if (!search_url.is_valid()) {
    RecordDesktopSearchURLAction(
        DESKTOP_SEARCH_URL_ACTION_NO_REDIRECTION_INVALID_SEARCH_ENGINE);
    return false;
  }

  RecordDesktopSearchURLAction(DESKTOP_SEARCH_URL_ACTION_REDIRECTION);

  url->Swap(&search_url);
  return !pref_service->GetBoolean(
      prefs::kDesktopSearchRedirectionInfobarShownPref);
}

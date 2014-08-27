// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_delegate.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"

ChromeAutocompleteProviderDelegate::ChromeAutocompleteProviderDelegate(
    Profile* profile)
    : profile_(profile),
      scheme_classifier_(profile) {
}

ChromeAutocompleteProviderDelegate::~ChromeAutocompleteProviderDelegate() {
}

net::URLRequestContextGetter*
ChromeAutocompleteProviderDelegate::RequestContext() {
  return profile_->GetRequestContext();
}

bool ChromeAutocompleteProviderDelegate::IsOffTheRecord() {
  return profile_->IsOffTheRecord();
}

std::string ChromeAutocompleteProviderDelegate::AcceptLanguages() {
  return profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

bool ChromeAutocompleteProviderDelegate::SearchSuggestEnabled() {
  return profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

bool ChromeAutocompleteProviderDelegate::ShowBookmarkBar() {
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

const AutocompleteSchemeClassifier&
ChromeAutocompleteProviderDelegate::SchemeClassifier() {
  return scheme_classifier_;
}

void ChromeAutocompleteProviderDelegate::Classify(
    const base::string16& text,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    metrics::OmniboxEventProto::PageClassification page_classification,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) {
  AutocompleteClassifier* classifier =
      AutocompleteClassifierFactory::GetForProfile(profile_);
  DCHECK(classifier);
  classifier->Classify(text, prefer_keyword, allow_exact_keyword_match,
                       page_classification, match, alternate_nav_url);
}

history::URLDatabase* ChromeAutocompleteProviderDelegate::InMemoryDatabase() {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  return history_service ? history_service->InMemoryDatabase() : NULL;
}

void
ChromeAutocompleteProviderDelegate::DeleteMatchingURLsForKeywordFromHistory(
    history::KeywordID keyword_id,
    const base::string16& term) {
  HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS)
      ->DeleteMatchingURLsForKeyword(keyword_id, term);
}

bool ChromeAutocompleteProviderDelegate::TabSyncEnabledAndUnencrypted() {
  // Check field trials and settings allow sending the URL on suggest requests.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  sync_driver::SyncPrefs sync_prefs(profile_->GetPrefs());
  return service &&
      service->IsSyncEnabledAndLoggedIn() &&
      sync_prefs.GetPreferredDataTypes(syncer::UserTypes()).Has(
          syncer::PROXY_TABS) &&
      !service->GetEncryptedDataTypes().Has(syncer::SESSIONS);
}

void ChromeAutocompleteProviderDelegate::PrefetchImage(const GURL& url) {
  BitmapFetcherService* image_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  DCHECK(image_service);
  image_service->Prefetch(url);
}

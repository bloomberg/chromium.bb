// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/autocomplete_provider_client_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/sync_service_utils.h"
#include "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "ios/chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "ios/chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"

AutocompleteProviderClientImpl::AutocompleteProviderClientImpl(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state), search_terms_data_(browser_state_) {}

AutocompleteProviderClientImpl::~AutocompleteProviderClientImpl() {}

net::URLRequestContextGetter*
AutocompleteProviderClientImpl::GetRequestContext() {
  return browser_state_->GetRequestContext();
}

PrefService* AutocompleteProviderClientImpl::GetPrefs() {
  return browser_state_->GetPrefs();
}

const AutocompleteSchemeClassifier&
AutocompleteProviderClientImpl::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier*
AutocompleteProviderClientImpl::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForBrowserState(browser_state_);
}

history::HistoryService* AutocompleteProviderClientImpl::GetHistoryService() {
  return ios::HistoryServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<history::TopSites> AutocompleteProviderClientImpl::GetTopSites() {
  return ios::TopSitesFactory::GetForBrowserState(browser_state_);
}

bookmarks::BookmarkModel* AutocompleteProviderClientImpl::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForBrowserState(browser_state_);
}

history::URLDatabase* AutocompleteProviderClientImpl::GetInMemoryDatabase() {
  // This method is called in unit test contexts where the HistoryService isn't
  // loaded. In that case, return null.
  history::HistoryService* history_service = GetHistoryService();
  return history_service ? history_service->InMemoryDatabase() : nullptr;
}

InMemoryURLIndex* AutocompleteProviderClientImpl::GetInMemoryURLIndex() {
  return ios::InMemoryURLIndexFactory::GetForBrowserState(browser_state_);
}

TemplateURLService* AutocompleteProviderClientImpl::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
}

const TemplateURLService*
AutocompleteProviderClientImpl::GetTemplateURLService() const {
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
}

const SearchTermsData& AutocompleteProviderClientImpl::GetSearchTermsData()
    const {
  return search_terms_data_;
}

scoped_refptr<ShortcutsBackend>
AutocompleteProviderClientImpl::GetShortcutsBackend() {
  return ios::ShortcutsBackendFactory::GetForBrowserState(browser_state_);
}

scoped_refptr<ShortcutsBackend>
AutocompleteProviderClientImpl::GetShortcutsBackendIfExists() {
  return ios::ShortcutsBackendFactory::GetForBrowserStateIfExists(
      browser_state_);
}

scoped_ptr<KeywordExtensionsDelegate>
AutocompleteProviderClientImpl::GetKeywordExtensionsDelegate(
    KeywordProvider* keyword_provider) {
  return nullptr;
}

std::string AutocompleteProviderClientImpl::GetAcceptLanguages() const {
  return browser_state_->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

std::string
AutocompleteProviderClientImpl::GetEmbedderRepresentationOfAboutScheme() {
  return kChromeUIScheme;
}

std::vector<base::string16> AutocompleteProviderClientImpl::GetBuiltinURLs() {
  // iOS does not supports BuiltinProvider.
  return std::vector<base::string16>();
}

std::vector<base::string16>
AutocompleteProviderClientImpl::GetBuiltinsToProvideAsUserTypes() {
  // iOS does not supports BuiltinProvider.
  return std::vector<base::string16>();
}

bool AutocompleteProviderClientImpl::IsOffTheRecord() const {
  return browser_state_->IsOffTheRecord();
}

bool AutocompleteProviderClientImpl::SearchSuggestEnabled() const {
  return browser_state_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

bool AutocompleteProviderClientImpl::TabSyncEnabledAndUnencrypted() const {
  return sync_driver::IsTabSyncEnabledAndUnencrypted(
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state_),
      browser_state_->GetPrefs());
}

void AutocompleteProviderClientImpl::Classify(
    const base::string16& text,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    metrics::OmniboxEventProto::PageClassification page_classification,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) {
  AutocompleteClassifier* classifier = GetAutocompleteClassifier();
  classifier->Classify(text, prefer_keyword, allow_exact_keyword_match,
                       page_classification, match, alternate_nav_url);
}

void AutocompleteProviderClientImpl::DeleteMatchingURLsForKeywordFromHistory(
    history::KeywordID keyword_id,
    const base::string16& term) {
  GetHistoryService()->DeleteMatchingURLsForKeyword(keyword_id, term);
}

void AutocompleteProviderClientImpl::PrefetchImage(const GURL& url) {}

void AutocompleteProviderClientImpl::OnAutocompleteControllerResultReady(
    AutocompleteController* controller) {
  // iOS currently has no client for this event.
}

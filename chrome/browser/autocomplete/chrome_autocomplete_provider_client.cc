// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <stddef.h>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "content/public/browser/notification_service.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/autocomplete/keyword_extensions_delegate_impl.h"
#endif

#if !defined(OS_ANDROID)
namespace {

// This list should be kept in sync with chrome/common/url_constants.h.
// Only include useful sub-pages, confirmation alerts are not useful.
const char* const kChromeSettingsSubPages[] = {
    chrome::kAutofillSubPage,
    chrome::kClearBrowserDataSubPage,
    chrome::kContentSettingsSubPage,
    chrome::kImportDataSubPage,
    chrome::kLanguageOptionsSubPage,
    chrome::kPasswordManagerSubPage,
    chrome::kResetProfileSettingsSubPage,
    chrome::kSearchEnginesSubPage,
    chrome::kSyncSetupSubPage,
#if defined(OS_CHROMEOS)
    chrome::kInternetOptionsSubPage,
#endif
};

}  // namespace
#endif  // !defined(OS_ANDROID)

ChromeAutocompleteProviderClient::ChromeAutocompleteProviderClient(
    Profile* profile)
    : profile_(profile),
      scheme_classifier_(profile),
      search_terms_data_(profile_) {
}

ChromeAutocompleteProviderClient::~ChromeAutocompleteProviderClient() {
}

net::URLRequestContextGetter*
ChromeAutocompleteProviderClient::GetRequestContext() {
  return profile_->GetRequestContext();
}

PrefService* ChromeAutocompleteProviderClient::GetPrefs() {
  return profile_->GetPrefs();
}

const AutocompleteSchemeClassifier&
ChromeAutocompleteProviderClient::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier*
ChromeAutocompleteProviderClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
}

history::HistoryService* ChromeAutocompleteProviderClient::GetHistoryService() {
  return HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<history::TopSites>
ChromeAutocompleteProviderClient::GetTopSites() {
  return TopSitesFactory::GetForProfile(profile_);
}

bookmarks::BookmarkModel* ChromeAutocompleteProviderClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

history::URLDatabase* ChromeAutocompleteProviderClient::GetInMemoryDatabase() {
  history::HistoryService* history_service = GetHistoryService();

  // This method is called in unit test contexts where the HistoryService isn't
  // loaded.
  return history_service ? history_service->InMemoryDatabase() : NULL;
}

InMemoryURLIndex* ChromeAutocompleteProviderClient::GetInMemoryURLIndex() {
  return InMemoryURLIndexFactory::GetForProfile(profile_);
}

TemplateURLService* ChromeAutocompleteProviderClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const TemplateURLService*
ChromeAutocompleteProviderClient::GetTemplateURLService() const {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const
SearchTermsData& ChromeAutocompleteProviderClient::GetSearchTermsData() const {
  return search_terms_data_;
}

scoped_refptr<ShortcutsBackend>
ChromeAutocompleteProviderClient::GetShortcutsBackend() {
  return ShortcutsBackendFactory::GetForProfile(profile_);
}

scoped_refptr<ShortcutsBackend>
ChromeAutocompleteProviderClient::GetShortcutsBackendIfExists() {
  return ShortcutsBackendFactory::GetForProfileIfExists(profile_);
}

std::unique_ptr<KeywordExtensionsDelegate>
ChromeAutocompleteProviderClient::GetKeywordExtensionsDelegate(
    KeywordProvider* keyword_provider) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return base::MakeUnique<KeywordExtensionsDelegateImpl>(profile_,
                                                         keyword_provider);
#else
  return nullptr;
#endif
}

physical_web::PhysicalWebDataSource*
ChromeAutocompleteProviderClient::GetPhysicalWebDataSource() {
  return nullptr;
}

std::string ChromeAutocompleteProviderClient::GetAcceptLanguages() const {
  return profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

std::string
ChromeAutocompleteProviderClient::GetEmbedderRepresentationOfAboutScheme() {
  return content::kChromeUIScheme;
}

std::vector<base::string16> ChromeAutocompleteProviderClient::GetBuiltinURLs() {
  std::vector<std::string> chrome_builtins(
      chrome::kChromeHostURLs,
      chrome::kChromeHostURLs + chrome::kNumberOfChromeHostURLs);
  std::sort(chrome_builtins.begin(), chrome_builtins.end());

  std::vector<base::string16> builtins;

  for (std::vector<std::string>::iterator i(chrome_builtins.begin());
       i != chrome_builtins.end(); ++i)
    builtins.push_back(base::ASCIIToUTF16(*i));

#if !defined(OS_ANDROID)
  base::string16 settings(base::ASCIIToUTF16(chrome::kChromeUISettingsHost) +
                          base::ASCIIToUTF16("/"));
  for (size_t i = 0; i < arraysize(kChromeSettingsSubPages); i++) {
    builtins.push_back(settings +
                       base::ASCIIToUTF16(kChromeSettingsSubPages[i]));
  }

  if (!base::FeatureList::IsEnabled(features::kMaterialDesignSettings)) {
    builtins.push_back(
        settings +
        base::ASCIIToUTF16(
            chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage));
  }
#endif

  return builtins;
}

std::vector<base::string16>
ChromeAutocompleteProviderClient::GetBuiltinsToProvideAsUserTypes() {
  std::vector<base::string16> builtins_to_provide;
  builtins_to_provide.push_back(
      base::ASCIIToUTF16(chrome::kChromeUIChromeURLsURL));
#if !defined(OS_ANDROID)
  builtins_to_provide.push_back(
      base::ASCIIToUTF16(chrome::kChromeUISettingsURL));
#endif
  builtins_to_provide.push_back(
      base::ASCIIToUTF16(chrome::kChromeUIVersionURL));
  return builtins_to_provide;
}

bool ChromeAutocompleteProviderClient::IsOffTheRecord() const {
  return profile_->IsOffTheRecord();
}

bool ChromeAutocompleteProviderClient::SearchSuggestEnabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

bool ChromeAutocompleteProviderClient::TabSyncEnabledAndUnencrypted() const {
  return syncer::IsTabSyncEnabledAndUnencrypted(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_),
      profile_->GetPrefs());
}

void ChromeAutocompleteProviderClient::Classify(
    const base::string16& text,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    metrics::OmniboxEventProto::PageClassification page_classification,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) {
  AutocompleteClassifier* classifier = GetAutocompleteClassifier();
  DCHECK(classifier);
  classifier->Classify(text, prefer_keyword, allow_exact_keyword_match,
                       page_classification, match, alternate_nav_url);
}

void ChromeAutocompleteProviderClient::DeleteMatchingURLsForKeywordFromHistory(
    history::KeywordID keyword_id,
    const base::string16& term) {
  GetHistoryService()->DeleteMatchingURLsForKeyword(keyword_id, term);
}

void ChromeAutocompleteProviderClient::PrefetchImage(const GURL& url) {
  BitmapFetcherService* image_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  DCHECK(image_service);
  image_service->Prefetch(url);
}

void ChromeAutocompleteProviderClient::OnAutocompleteControllerResultReady(
    AutocompleteController* controller) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
      content::Source<AutocompleteController>(controller),
      content::NotificationService::NoDetails());
}

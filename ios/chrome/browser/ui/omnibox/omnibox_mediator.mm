// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kOmniboxIconSize = 16;
}  // namespace

@interface OmniboxMediator () <SearchEngineObserving>

// Whether the current default search engine supports search-by-image.
@property(nonatomic, assign) BOOL searchEngineSupportsSearchByImage;

// The latest URL used to fetch the favicon.
@property(nonatomic, assign) GURL latestFaviconURL;
// The latest URL used to fetch the default search engine favicon.
@property(nonatomic, assign) const TemplateURL* latestDefaultSearchEngine;

@end

@implementation OmniboxMediator {
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _searchEngineSupportsSearchByImage = NO;
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<OmniboxConsumer>)consumer {
  _consumer = consumer;
  [consumer
      updateSearchByImageSupported:self.searchEngineSupportsSearchByImage];

  if (base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    // Show Default Search Engine favicon.
    // Remember what is the Default Search Engine provider that the icon is for,
    // in case the user changes Default Search Engine while this is being
    // loaded.
    __weak OmniboxMediator* weakSelf = self;
    const TemplateURL* defaultSearchEngine =
        _templateURLService->GetDefaultSearchProvider();
    self.latestDefaultSearchEngine = defaultSearchEngine;
    [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* image) {
      if (weakSelf.latestDefaultSearchEngine == defaultSearchEngine) {
        [weakSelf.consumer setEmptyTextLeadingImage:image];
      }
    }];
  }
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(templateURLService);
  _searchEngineObserver =
      std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
}

- (void)setSearchEngineSupportsSearchByImage:
    (BOOL)searchEngineSupportsSearchByImage {
  BOOL supportChanged = self.searchEngineSupportsSearchByImage !=
                        searchEngineSupportsSearchByImage;
  _searchEngineSupportsSearchByImage = searchEngineSupportsSearchByImage;
  if (supportChanged) {
    [self.consumer
        updateSearchByImageSupported:searchEngineSupportsSearchByImage];
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(self.templateURLService);
}

#pragma mark - OmniboxLeftImageConsumer

- (void)setLeftImageForAutocompleteType:(AutocompleteMatchType::Type)matchType
                             answerType:
                                 (base::Optional<SuggestionAnswer::AnswerType>)
                                     answerType
                             faviconURL:(GURL)faviconURL {
  UIImage* image = GetOmniboxSuggestionIconForAutocompleteMatchType(
      matchType, /* is_starred */ false);
  [self.consumer updateAutocompleteIcon:image];

  if (base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    __weak OmniboxMediator* weakSelf = self;
    if (AutocompleteMatch::IsSearchType(matchType)) {
      // Show Default Search Engine favicon.
      // Remember what is the Default Search Engine provider that the icon is
      // for, in case the user changes Default Search Engine while this is being
      // loaded.
      const TemplateURL* defaultSearchEngine =
          _templateURLService->GetDefaultSearchProvider();
      self.latestDefaultSearchEngine = defaultSearchEngine;
      [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* image) {
        if (weakSelf.latestDefaultSearchEngine == defaultSearchEngine) {
          [weakSelf.consumer updateAutocompleteIcon:image];
        }
      }];
    } else {
      // Show favicon.
      // Remember which favicon is loaded in case we start loading a new one
      // before this one completes.
      self.latestFaviconURL = faviconURL;
      [self loadFaviconByPageURL:faviconURL
                      completion:^(UIImage* image) {
                        if (weakSelf.latestFaviconURL == faviconURL) {
                          [weakSelf.consumer updateAutocompleteIcon:image];
                        }
                      }];
    }
  }
}

// Loads a favicon for a given page URL.
// |pageURL| is url for the page that needs a favicon
// |completion| handler might be called multiple
// times, synchronously and asynchronously. It will always be called on the main
// thread.
- (void)loadFaviconByPageURL:(GURL)pageURL
                  completion:(void (^)(UIImage* image))completion {
  // Can't load favicons without a favicon loader.
  DCHECK(self.faviconLoader);

  auto handleFaviconResult = ^void(FaviconAttributes* faviconCacheResult) {
    if (faviconCacheResult.faviconImage &&
        !faviconCacheResult.usesDefaultImage) {
      if (completion) {
        completion(faviconCacheResult.faviconImage);
      }
    }
  };

  // Remember the last request URL to avoid showing favicons for past requests.
  self.latestFaviconURL = pageURL;
  // Download the favicon.
  // The code below mimics that in OmniboxPopupMediator.
  FaviconAttributes* faviconCacheResult = self.faviconLoader->FaviconForPageUrl(
      pageURL, kOmniboxIconSize, kOmniboxIconSize,
      /*fallback_to_google_server=*/YES, handleFaviconResult);
  // Handle the synchronously returned cache result. If the favicon loader did
  // an async fetch, |handleFaviconResult| may be called again later.
  handleFaviconResult(faviconCacheResult);
}

// Loads a favicon for the current default search engine.
// |completion| handler might be called multiple times, synchronously
// and asynchronously. It will always be called on the main
// thread.
- (void)loadDefaultSearchEngineFaviconWithCompletion:
    (void (^)(UIImage* image))completion {
  // Can't load favicons without a favicon loader.
  DCHECK(self.faviconLoader);
  DCHECK(base::FeatureList::IsEnabled(kOmniboxUseDefaultSearchEngineFavicon));

  const TemplateURL* defaultProvider =
      _templateURLService->GetDefaultSearchProvider();

  // Prepopulated search engines don't have a favicon URL, so the favicon is
  // loaded with an empty query search page URL.
  if (defaultProvider->prepopulate_id() != 0) {
    // Fake up a page URL for favicons of prepopulated search engines, since
    // favicons may be fetched from Google server which doesn't suppoprt
    // icon URL.
    std::string emptyPageUrl = defaultProvider->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(base::string16()),
        _templateURLService->search_terms_data());
    [self loadFaviconByPageURL:GURL(emptyPageUrl) completion:completion];
    return;
  }

  auto handleFaviconResult = ^void(FaviconAttributes* faviconCacheResult) {
    if (faviconCacheResult.faviconImage &&
        !faviconCacheResult.usesDefaultImage) {
      if (completion) {
        completion(faviconCacheResult.faviconImage);
      }
    }
  };

  // Download the favicon.
  // The code below mimics that in OmniboxPopupMediator.
  FaviconAttributes* faviconCacheResult = self.faviconLoader->FaviconForIconUrl(
      defaultProvider->favicon_url(), kOmniboxIconSize, kOmniboxIconSize,
      handleFaviconResult);
  handleFaviconResult(faviconCacheResult);
}

@end

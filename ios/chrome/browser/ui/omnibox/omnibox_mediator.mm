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

  if (!base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    return;
  }

  // Can't load favicons without a favicon loader.
  DCHECK(self.faviconLoader);

  BOOL showsDefaultSearchEngineFavicon =
      base::FeatureList::IsEnabled(kOmniboxUseDefaultSearchEngineFavicon) &&
      AutocompleteMatch::IsSearchType(matchType);

  const TemplateURL* default_provider =
      _templateURLService->GetDefaultSearchProvider();
  // Prepopulated search engines use empty search URL for favicon retrieval;
  // custom search engines use a direct favicon URL.
  BOOL shouldUseFaviconURL = default_provider->prepopulate_id() == 0;

  if (showsDefaultSearchEngineFavicon) {
    if (shouldUseFaviconURL) {
      faviconURL = default_provider->favicon_url();
    } else {
      // Fake up a page URL for favicons of prepopulated search engines, since
      // favicons may be fetched from Google server which doesn't suppoprt
      // icon URL.
      std::string emptyPageUrl = default_provider->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(base::string16()),
          _templateURLService->search_terms_data());
      faviconURL = GURL(emptyPageUrl);
    }
  }

  __weak OmniboxMediator* weakSelf = self;

  auto useFaviconAttributes = ^void(FaviconAttributes* attributes) {
    if (attributes.faviconImage && !attributes.usesDefaultImage &&
        weakSelf.latestFaviconURL == faviconURL)
      [weakSelf.consumer updateAutocompleteIcon:attributes.faviconImage];
  };

  // Remember the last request URL to avoid showing favicons for past requests.
  self.latestFaviconURL = faviconURL;
  // Download the favicon.
  // The code below mimics that in OmniboxPopupMediator.
  FaviconAttributes* cachedAttributes = nil;
  if (shouldUseFaviconURL) {
    cachedAttributes = self.faviconLoader->FaviconForIconUrl(
        faviconURL, kOmniboxIconSize, kOmniboxIconSize, useFaviconAttributes);
  } else {
    cachedAttributes = self.faviconLoader->FaviconForPageUrl(
        faviconURL, kOmniboxIconSize, kOmniboxIconSize,
        /*fallback_to_google_server=*/YES, useFaviconAttributes);
  }

  useFaviconAttributes(cachedAttributes);
}

@end

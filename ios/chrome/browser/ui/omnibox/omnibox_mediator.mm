// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_mediator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxMediator () <SearchEngineObserving>

// Whether the current default search engine supports search-by-image.
@property(nonatomic, assign) BOOL searchEngineSupportsSearchByImage;

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

- (void)setLeftImageForAutocompleteType:(AutocompleteMatchType::Type)type {
  UIImage* image = GetOmniboxSuggestionIconForAutocompleteMatchType(
      type, /* is_starred */ false);
  [self.consumer updateAutocompleteIcon:image];
}

@end

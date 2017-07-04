// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_favicon_mediator.h"

#include "base/mac/bind_objc_block.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Size of the favicon returned by the provider for the suggestions items.
const CGFloat kSuggestionsFaviconSize = 16;
// Size of the favicon returned by the provider for the most visited items.
const CGFloat kMostVisitedFaviconSize = 48;
// Size below which the provider returns a colored tile instead of an image.
const CGFloat kMostVisitedFaviconMinimalSize = 32;

}  // namespace

@interface ContentSuggestionsFaviconMediator ()

// The ContentSuggestionsService, serving suggestions.
@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* contentService;

// FaviconAttributesProvider to fetch the favicon for the most visited tiles.
@property(nonatomic, nullable, strong)
    FaviconAttributesProvider* mostVisitedAttributesProvider;
// FaviconAttributesProvider to fetch the favicon for the suggestions.
@property(nonatomic, nullable, strong)
    FaviconAttributesProvider* suggestionsAttributesProvider;

@end

@implementation ContentSuggestionsFaviconMediator

@synthesize mostVisitedAttributesProvider = _mostVisitedAttributesProvider;
@synthesize suggestionsAttributesProvider = _suggestionsAttributesProvider;
@synthesize contentService = _contentService;
@synthesize dataSink = _dataSink;

#pragma mark - Public.

- (instancetype)
initWithContentService:(ntp_snippets::ContentSuggestionsService*)contentService
      largeIconService:(favicon::LargeIconService*)largeIconService {
  self = [super init];
  if (self) {
    _mostVisitedAttributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kMostVisitedFaviconSize
             minFaviconSize:kMostVisitedFaviconMinimalSize
           largeIconService:largeIconService];
    _suggestionsAttributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kSuggestionsFaviconSize
             minFaviconSize:1
           largeIconService:largeIconService];
    _contentService = contentService;
  }
  return self;
}

- (void)fetchFaviconForMostVisited:(ContentSuggestionsMostVisitedItem*)item {
  __weak ContentSuggestionsFaviconMediator* weakSelf = self;
  __weak ContentSuggestionsMostVisitedItem* weakItem = item;

  void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
    ContentSuggestionsFaviconMediator* strongSelf = weakSelf;
    ContentSuggestionsMostVisitedItem* strongItem = weakItem;
    if (!strongSelf || !strongItem)
      return;

    strongItem.attributes = attributes;
    [strongSelf.dataSink itemHasChanged:strongItem];
  };

  [self.mostVisitedAttributesProvider fetchFaviconAttributesForURL:item.URL
                                                        completion:completion];
}

- (void)fetchFaviconForSuggestions:(ContentSuggestionsItem*)item
                        inCategory:(ntp_snippets::Category)category {
  __weak ContentSuggestionsFaviconMediator* weakSelf = self;
  __weak ContentSuggestionsItem* weakItem = item;

  void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
    ContentSuggestionsFaviconMediator* strongSelf = weakSelf;
    ContentSuggestionsItem* strongItem = weakItem;
    if (!strongSelf || !strongItem)
      return;

    strongItem.attributes = attributes;
    [strongSelf.dataSink itemHasChanged:strongItem];
    [strongSelf fetchFaviconImageForSuggestions:strongItem inCategory:category];
  };

  [self.suggestionsAttributesProvider fetchFaviconAttributesForURL:item.URL
                                                        completion:completion];
}

#pragma mark - Private.

// Fetches the favicon image for the |item|, based on the
// ContentSuggestionsService.
- (void)fetchFaviconImageForSuggestions:(ContentSuggestionsItem*)item
                             inCategory:(ntp_snippets::Category)category {
  if (!category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES)) {
    // TODO(crbug.com/721266): remove this guard once the choice to download the
    // favicon from the google server is done in the provider.
    return;
  }

  __weak ContentSuggestionsFaviconMediator* weakSelf = self;
  __weak ContentSuggestionsItem* weakItem = item;
  void (^imageCallback)(const gfx::Image&) = ^(const gfx::Image& image) {
    ContentSuggestionsFaviconMediator* strongSelf = weakSelf;
    ContentSuggestionsItem* strongItem = weakItem;
    if (!strongSelf || !strongItem || image.IsEmpty())
      return;

    strongItem.attributes =
        [FaviconAttributes attributesWithImage:[image.ToUIImage() copy]];
    [strongSelf.dataSink itemHasChanged:strongItem];
  };

  ntp_snippets::ContentSuggestion::ID identifier =
      ntp_snippets::ContentSuggestion::ID(
          category, item.suggestionIdentifier.IDInSection);
  self.contentService->FetchSuggestionFavicon(
      identifier, /* minimum_size_in_pixel = */ 1, kSuggestionsFaviconSize,
      base::BindBlockArc(imageCallback));
}

@end

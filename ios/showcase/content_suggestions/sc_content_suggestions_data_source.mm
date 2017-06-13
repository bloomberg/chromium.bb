// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/content_suggestions/sc_content_suggestions_data_source.h"

#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_fetcher.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/showcase/content_suggestions/sc_content_suggestions_item.h"
#import "ios/showcase/content_suggestions/sc_content_suggestions_most_visited_item.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/l10n/time_format.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
}

@interface SCContentSuggestionsDataSource ()<ContentSuggestionsImageFetcher>

// Section Info of type MostVisited. Created lazily.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* mostVisitedSection;
// Section Info of type Reading List. Created lazily.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* readingListSection;
// Section Info of type Article. Created lazily.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* articleSection;

@end

@implementation SCContentSuggestionsDataSource

@synthesize dataSink = _dataSink;
@synthesize mostVisitedSection = _mostVisitedSection;
@synthesize readingListSection = _readingListSection;
@synthesize articleSection = _articleSection;

#pragma mark - Public

+ (NSString*)titleFirstSuggestion {
  return @"Title of the first suggestions";
}

+ (NSString*)titleReadingListItem {
  return @"Reading List item: No subtitle, no image";
}

#pragma mark - ContentSuggestionsDataSource

- (NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo {
  return @[
    self.mostVisitedSection, self.readingListSection, self.articleSection
  ];
}

- (NSArray<CSCollectionViewItem*>*)itemsForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  switch (sectionInfo.sectionID) {
    case ContentSuggestionsSectionMostVisited: {
      NSMutableArray<CSCollectionViewItem*>* items = [NSMutableArray array];
      for (int i = 0; i < 8; i++) {
        SCContentSuggestionsMostVisitedItem* suggestion =
            [[SCContentSuggestionsMostVisitedItem alloc] initWithType:0];
        suggestion.title =
            [NSString stringWithFormat:@"Test Tile number %i", i];
        suggestion.attributes = [self randomColorFaviconAttributes];
        [items addObject:suggestion];
      }
      return items;
    }
    case ContentSuggestionsSectionReadingList: {
      SCContentSuggestionsItem* suggestion = [self article];
      suggestion.title = [[self class] titleReadingListItem];
      suggestion.subtitle = nil;
      suggestion.hasImage = NO;
      suggestion.suggestionIdentifier.sectionInfo = self.readingListSection;
      return @[ suggestion ];
    }
    case ContentSuggestionsSectionArticles: {
      NSMutableArray<CSCollectionViewItem*>* items = [NSMutableArray array];
      SCContentSuggestionsItem* suggestion = [self article];
      suggestion.title = [[self class] titleFirstSuggestion];
      suggestion.suggestionIdentifier.IDInSection = std::to_string(1);
      [items addObject:suggestion];
      for (int i = 2; i < 6; i++) {
        suggestion = suggestion = [self article];
        suggestion.title = [NSString
            stringWithFormat:@"Title of the suggestions number #%i", i];
        suggestion.suggestionIdentifier.IDInSection = std::to_string(i);
        [items addObject:suggestion];
      }
      return items;
    }
    case ContentSuggestionsSectionUnknown:
      return @[];
  }
}

- (id<ContentSuggestionsImageFetcher>)imageFetcher {
  return self;
}

- (void)fetchMoreSuggestionsKnowing:
            (NSArray<ContentSuggestionIdentifier*>*)knownSuggestions
                    fromSectionInfo:
                        (ContentSuggestionsSectionInformation*)sectionInfo
                           callback:(MoreSuggestionsFetched)callback {
}

- (void)fetchFaviconAttributesForItem:(CSCollectionViewItem*)item
                           completion:(void (^)(FaviconAttributes*))completion {
}

- (void)fetchFaviconImageForItem:(CSCollectionViewItem*)item
                      completion:(void (^)(UIImage*))completion {
}

- (void)dismissSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier {
}

#pragma mark - ContentSuggestionsImageFetcher

- (void)fetchImageForSuggestion:
            (ContentSuggestionIdentifier*)suggestionIdentifier
                       callback:(void (^)(UIImage*))callback {
  if (callback) {
    callback([UIImage imageNamed:@"reading_list_empty_state"]);
  }
}

#pragma mark - Property

- (ContentSuggestionsSectionInformation*)mostVisitedSection {
  if (!_mostVisitedSection) {
    _mostVisitedSection = [[ContentSuggestionsSectionInformation alloc]
        initWithSectionID:ContentSuggestionsSectionMostVisited];
    _mostVisitedSection.layout = ContentSuggestionsSectionLayoutCustom;
  }
  return _mostVisitedSection;
}

- (ContentSuggestionsSectionInformation*)readingListSection {
  if (!_readingListSection) {
    _readingListSection = [[ContentSuggestionsSectionInformation alloc]
        initWithSectionID:ContentSuggestionsSectionReadingList];
    _readingListSection.title =
        l10n_util::GetNSString(IDS_NTP_READING_LIST_SUGGESTIONS_SECTION_HEADER);
    _readingListSection.emptyText =
        l10n_util::GetNSString(IDS_NTP_READING_LIST_SUGGESTIONS_SECTION_EMPTY);
    _readingListSection.footerTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE);
  }
  return _readingListSection;
}

- (ContentSuggestionsSectionInformation*)articleSection {
  if (!_articleSection) {
    _articleSection = [[ContentSuggestionsSectionInformation alloc]
        initWithSectionID:ContentSuggestionsSectionArticles];
    _articleSection.title =
        l10n_util::GetNSString(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER);
    _articleSection.footerTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE);
  }
  return _articleSection;
}

#pragma mark - Private

// Returns favicon attributes with a random background color and a random
// monogram.
- (FaviconAttributes*)randomColorFaviconAttributes {
  UIColor* backgroundColor = [[UIColor alloc] initWithHue:drand48()
                                               saturation:drand48() / 2 + 0.5
                                               brightness:drand48() / 2 + 0.5
                                                    alpha:1.0];

  NSString* monogram =
      [NSString stringWithFormat:@"%c", arc4random_uniform(26) + 'A'];
  UIColor* textColor =
      drand48() > 0.5 ? [UIColor whiteColor] : [UIColor blackColor];

  return [FaviconAttributes attributesWithMonogram:monogram
                                         textColor:textColor
                                   backgroundColor:backgroundColor
                            defaultBackgroundColor:NO];
}

// Returns a random date between now and three days before now.
- (NSString*)randomDate {
  int offset = arc4random_uniform(259200);
  return base::SysUTF16ToNSString(ui::TimeFormat::SimpleWithMonthAndYear(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
      base::TimeDelta::FromSeconds(offset + 60), true));
}

// Returns an article with the fields set except the IDInSection.
- (SCContentSuggestionsItem*)article {
  SCContentSuggestionsItem* suggestion =
      [[SCContentSuggestionsItem alloc] initWithType:0];
  suggestion.title = @"Title";
  suggestion.subtitle = @"Subtitle for this greeeeeaaaaaat suggestion!";
  suggestion.publisher = @"Publisher of the new";
  suggestion.hasImage = YES;
  suggestion.publicationDate = [self randomDate];
  suggestion.attributes = [self randomColorFaviconAttributes];
  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.sectionInfo = self.articleSection;
  return suggestion;
}

@end

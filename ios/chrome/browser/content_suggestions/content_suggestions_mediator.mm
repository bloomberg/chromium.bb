// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"

#include "base/memory/ptr_util.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestion.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_service_bridge_observer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestion.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsMediator ()<ContentSuggestionsServiceObserver> {
  // Bridge for this class to become an observer of a ContentSuggestionsService.
  std::unique_ptr<ContentSuggestionsServiceBridge> _suggestionBridge;
}

@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* contentService;

// Converts the data in |category| to ContentSuggestion and adds them to the
// |contentArray|.
- (void)addContentInCategory:(ntp_snippets::Category&)category
                     toArray:(NSMutableArray<ContentSuggestion*>*)contentArray;

// Converts the |contentsuggestion| to a ContentSuggestion.
- (ContentSuggestion*)convertContentSuggestion:
    (const ntp_snippets::ContentSuggestion&)contentsuggestion;

@end

@implementation ContentSuggestionsMediator

@synthesize contentService = _contentService;
@synthesize dataSink = _dataSink;

- (instancetype)initWithContentService:
    (ntp_snippets::ContentSuggestionsService*)contentService {
  self = [super init];
  if (self) {
    _suggestionBridge =
        base::MakeUnique<ContentSuggestionsServiceBridge>(self, contentService);
    _contentService = contentService;
  }
  return self;
}

#pragma mark - ContentSuggestionsDataSource

- (NSArray<ContentSuggestion*>*)allSuggestions {
  std::vector<ntp_snippets::Category> categories =
      self.contentService->GetCategories();
  NSMutableArray<ContentSuggestion*>* dataHolders = [NSMutableArray array];
  for (auto& category : categories) {
    [self addContentInCategory:category toArray:dataHolders];
  }
  return dataHolders;
}

#pragma mark - ContentSuggestionsServiceObserver

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
         newSuggestionsInCategory:(ntp_snippets::Category)category {
  [self.dataSink dataAvailable];
}

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
                         category:(ntp_snippets::Category)category
                  statusChangedTo:(ntp_snippets::CategoryStatus)status {
  // Update dataSink.
}

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
            SuggestionInvalidated:
                (const ntp_snippets::ContentSuggestion::ID&)suggestion_id {
  // Update dataSink.
}

- (void)contentSuggestionsServiceFullRefreshRequired:
    (ntp_snippets::ContentSuggestionsService*)suggestionsService {
  // Update dataSink.
}

- (void)contentSuggestionsServiceShutdown:
    (ntp_snippets::ContentSuggestionsService*)suggestionsService {
  // Update dataSink.
}

#pragma mark - Private

- (void)addContentInCategory:(ntp_snippets::Category&)category
                     toArray:(NSMutableArray<ContentSuggestion*>*)contentArray {
  const std::vector<ntp_snippets::ContentSuggestion>& suggestions =
      self.contentService->GetSuggestionsForCategory(category);
  for (auto& contentSuggestion : suggestions) {
    [contentArray addObject:[self convertContentSuggestion:contentSuggestion]];
  }
}

- (ContentSuggestion*)convertContentSuggestion:
    (const ntp_snippets::ContentSuggestion&)contentsuggestion {
  return [[ContentSuggestion alloc] init];
}

@end

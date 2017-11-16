// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_popup_mediator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#import "ios/chrome/browser/ui/omnibox/autocomplete_match_formatter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OmniboxPopupMediator {
  // Fetcher for Answers in Suggest images.
  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> _imageFetcher;

  OmniboxPopupMediatorDelegate* _delegate;  // weak

  AutocompleteResult _currentResult;
}
@synthesize consumer = _consumer;
@synthesize incognito = _incognito;

- (instancetype)initWithFetcher:
                    (std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper>)
                        imageFetcher
                       delegate:(OmniboxPopupMediatorDelegate*)delegate {
  self = [super init];
  if (self) {
    DCHECK(delegate);
    _delegate = delegate;
    _imageFetcher = std::move(imageFetcher);
  }
  return self;
}

- (void)updateMatches:(const AutocompleteResult&)result
        withAnimation:(BOOL)animation {
  _currentResult.Reset();
  _currentResult.CopyFrom(result);

  [self.consumer updateMatches:[self wrappedMatches] withAnimation:animation];
}

- (NSArray<id<AutocompleteSuggestion>>*)wrappedMatches {
  NSMutableArray<id<AutocompleteSuggestion>>* wrappedMatches =
      [[NSMutableArray alloc] init];

  size_t size = _currentResult.size();
  for (size_t i = 0; i < size; i++) {
    const AutocompleteMatch& match =
        ((const AutocompleteResult&)_currentResult).match_at((NSUInteger)i);
    AutocompleteMatchFormatter* formatter =
        [AutocompleteMatchFormatter formatterWithMatch:match];
    formatter.starred = _delegate->IsStarredMatch(match);
    formatter.incognito = _incognito;
    [wrappedMatches addObject:formatter];
  }

  return wrappedMatches;
}

#pragma mark - AutocompleteResultConsumerDelegate

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                      didSelectRow:(NSUInteger)row {
  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);

  _delegate->OnMatchSelected(match, row);
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
          didSelectRowForAppending:(NSUInteger)row {
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);

  if (AutocompleteMatch::IsSearchType(match.type)) {
    base::RecordAction(
        base::UserMetricsAction("MobileOmniboxRefineSuggestion.Search"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileOmniboxRefineSuggestion.Url"));
  }

  _delegate->OnMatchSelectedForAppending(match);
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
           didSelectRowForDeletion:(NSUInteger)row {
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);
  _delegate->OnMatchSelectedForDeletion(match);
}

- (void)autocompleteResultConsumerDidScroll:
    (id<AutocompleteResultConsumer>)sender {
  _delegate->OnScroll();
}

#pragma mark - ImageFetcher

- (void)fetchImage:(GURL)imageURL completion:(void (^)(UIImage*))completion {
  image_fetcher::IOSImageDataFetcherCallback callback =
      ^(NSData* data, const image_fetcher::RequestMetadata& metadata) {
        if (data) {
          UIImage* image =
              [UIImage imageWithData:data scale:[UIScreen mainScreen].scale];
          completion(image);
        } else {
          completion(nil);
        }
      };
  _imageFetcher->FetchImageDataWebpDecoded(imageURL, callback);
}

@end

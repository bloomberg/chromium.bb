// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MEDIATOR_H_

#import <UIKit/UIKit.h>
#include "components/omnibox/browser/autocomplete_result.h"

#import "ios/chrome/browser/ui/omnibox/autocomplete_result_consumer.h"
#import "ios/chrome/browser/ui/omnibox/image_retriever.h"

namespace image_fetcher {
class IOSImageDataFetcherWrapper;
}  // namespace

class OmniboxPopupMediatorDelegate {
 public:
  virtual bool IsStarredMatch(const AutocompleteMatch& match) const = 0;
  virtual void OnMatchSelected(const AutocompleteMatch& match, size_t row) = 0;
  virtual void OnMatchSelectedForAppending(const AutocompleteMatch& match) = 0;
  virtual void OnMatchSelectedForDeletion(const AutocompleteMatch& match) = 0;
  virtual void OnScroll() = 0;
};

@interface OmniboxPopupMediator
    : NSObject<AutocompleteResultConsumerDelegate, ImageRetriever>

// Designated initializer. Takes ownership of |imageFetcher|.
- (instancetype)initWithFetcher:
                    (std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper>)
                        imageFetcher
                       delegate:(OmniboxPopupMediatorDelegate*)delegate;

- (void)updateMatches:(const AutocompleteResult&)result
        withAnimation:(BOOL)animated;

@property(nonatomic, weak) id<AutocompleteResultConsumer> consumer;
@property(nonatomic, assign, getter=isIncognito) BOOL incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MEDIATOR_H_

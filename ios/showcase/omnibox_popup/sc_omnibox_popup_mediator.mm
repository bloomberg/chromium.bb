// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_mediator.h"

#import "ios/chrome/browser/ui/omnibox/autocomplete_result_consumer.h"
#import "ios/showcase/omnibox_popup/fake_autocomplete_suggestion.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCOmniboxPopupMediator ()

@property(nonatomic, readonly, weak) id<AutocompleteResultConsumer> consumer;

@end

@implementation SCOmniboxPopupMediator

- (instancetype)initWithConsumer:(id<AutocompleteResultConsumer>)consumer {
  self = [super init];
  if (self) {
    _consumer = consumer;
  }
  return self;
}

- (void)updateMatches {
  FakeAutocompleteSuggestion* suggestion1 =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion1.supportsDeletion = YES;
  suggestion1.text = [[NSAttributedString alloc] initWithString:@"Match 1"];

  FakeAutocompleteSuggestion* suggestion2 =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion2.supportsDeletion = YES;
  suggestion2.text = [[NSAttributedString alloc] initWithString:@"Match 2"];
  suggestion2.detailText =
      [[NSAttributedString alloc] initWithString:@"Detail"];

  [self.consumer updateMatches:@[ suggestion1, suggestion2 ] withAnimation:YES];
}

@end

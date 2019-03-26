// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_mediator.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_result_consumer.h"
#import "ios/showcase/omnibox_popup/fake_autocomplete_suggestion.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Spacer attributed string for dividing parts of an autocomplete suggestion's
// text and detail text.
NSAttributedString* spacer() {
  return [[NSAttributedString alloc] initWithString:@"  "];
}

// Detail text for an autocomplete suggestion representing weather
NSAttributedString* weatherDetailText() {
  NSAttributedString* number = [[NSAttributedString alloc]
      initWithString:@"18"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:24],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSAttributedString* degreeSymbol = [[NSAttributedString alloc]
      initWithString:@"°C"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:12],
            NSBaselineOffsetAttributeName : @10.0f,
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSAttributedString* date = [[NSAttributedString alloc]
      initWithString:@"ven."
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:12],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];

  NSMutableAttributedString* answer =
      [[NSMutableAttributedString alloc] initWithAttributedString:number];
  [answer appendAttributedString:degreeSymbol];
  [answer appendAttributedString:spacer()];
  [answer appendAttributedString:date];

  return [answer copy];
}

// Main text for an autocomplete suggestion representing stock price
NSAttributedString* stockText() {
  NSAttributedString* search = [[NSAttributedString alloc]
      initWithString:@"goog stock"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
  NSAttributedString* priceSource = [[NSAttributedString alloc]
      initWithString:@"GOOG (NASDAQ), 13:18 UTC−4"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:12],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSMutableAttributedString* answer =
      [[NSMutableAttributedString alloc] initWithAttributedString:search];
  [answer appendAttributedString:spacer()];
  [answer appendAttributedString:priceSource];
  return [answer copy];
}

// Detail text for an autocomplete suggestion representing stock price
NSAttributedString* stockDetailText() {
  NSAttributedString* price = [[NSAttributedString alloc]
      initWithString:@"1 209,29"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:24],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSAttributedString* priceChange = [[NSAttributedString alloc]
      initWithString:@"-22,25 (-1,81%)"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
            NSForegroundColorAttributeName : [UIColor colorWithRed:197 / 255.0
                                                             green:57 / 255.0
                                                              blue:41 / 255.0
                                                             alpha:1.0],
          }];
  NSMutableAttributedString* answer =
      [[NSMutableAttributedString alloc] initWithAttributedString:price];
  [answer appendAttributedString:spacer()];
  [answer appendAttributedString:priceChange];
  return [answer copy];
}

// Main text for an autocomplete suggestion representing a word definition
NSAttributedString* definitionText() {
  NSAttributedString* searchText = [[NSAttributedString alloc]
      initWithString:@"define government"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
  NSAttributedString* pronunciation = [[NSAttributedString alloc]
      initWithString:@"• /ˈɡʌv(ə)nˌm(ə)nt/"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:14],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSMutableAttributedString* answer =
      [[NSMutableAttributedString alloc] initWithAttributedString:searchText];
  [answer appendAttributedString:spacer()];
  [answer appendAttributedString:pronunciation];
  return [answer copy];
}

// Detail text for an autocomplete suggestion representing a word definition
NSAttributedString* definitionDetailText() {
  return [[NSAttributedString alloc]
      initWithString:@"the group of people with the authority to govern a "
                     @"country or state; a particular ministry in office. "
                     @"Let's expand this definition to get to three lines also."
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:14],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
}
}  // namespace

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

// Creates many fake suggestions and passes them along to the
// AutocompleteResultConsumer.
- (void)updateMatches {
  FakeAutocompleteSuggestion* simpleSuggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  simpleSuggestion.text =
      [[NSAttributedString alloc] initWithString:@"Simple suggestion"];

  FakeAutocompleteSuggestion* suggestionWithDetail =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestionWithDetail.text =
      [[NSAttributedString alloc] initWithString:@"Suggestion with detail"];
  suggestionWithDetail.detailText =
      [[NSAttributedString alloc] initWithString:@"Detail"];

  FakeAutocompleteSuggestion* clippingSuggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  clippingSuggestion.text = [[NSAttributedString alloc]
      initWithString:@"Suggestion with text that clips because it is very long "
                     @"and extends off the right end of the screen"];
  clippingSuggestion.detailText = [[NSAttributedString alloc]
      initWithString:
          @"Detail about the suggestion that also clips because it is too long "
          @"for the screen and extends off of the right edge."];

  FakeAutocompleteSuggestion* appendableSuggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  appendableSuggestion.text =
      [[NSAttributedString alloc] initWithString:@"Appendable suggestion"];
  appendableSuggestion.isAppendable = true;

  FakeAutocompleteSuggestion* otherTabSuggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  otherTabSuggestion.text =
      [[NSAttributedString alloc] initWithString:@"Other tab suggestion"];
  otherTabSuggestion.isTabMatch = true;

  FakeAutocompleteSuggestion* deletableSuggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  deletableSuggestion.text =
      [[NSAttributedString alloc] initWithString:@"Deletable suggestion"];
  deletableSuggestion.supportsDeletion = YES;

  FakeAutocompleteSuggestion* weatherSuggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  weatherSuggestion.text =
      [[NSAttributedString alloc] initWithString:@"weather"];
  weatherSuggestion.hasAnswer = YES;
  weatherSuggestion.detailText = weatherDetailText();
  // The image currently doesn't display because there is no fake
  // Image Retriever, but leaving this here in case this is ever necessary.
  weatherSuggestion.imageURL =
      GURL("https://ssl.gstatic.com/onebox/weather/128/sunny.png");

  FakeAutocompleteSuggestion* stockSuggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  stockSuggestion.text = stockText();
  stockSuggestion.hasAnswer = YES;
  stockSuggestion.detailText = stockDetailText();

  FakeAutocompleteSuggestion* definitionSuggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  definitionSuggestion.text = definitionText();
  definitionSuggestion.numberOfLines = 3;
  definitionSuggestion.hasAnswer = YES;
  definitionSuggestion.detailText = definitionDetailText();

  NSArray<id<AutocompleteSuggestion>>* suggestions = @[
    simpleSuggestion,
    suggestionWithDetail,
    clippingSuggestion,
    appendableSuggestion,
    otherTabSuggestion,
    deletableSuggestion,
    stockSuggestion,
    weatherSuggestion,
    definitionSuggestion,
  ];

  [self.consumer updateMatches:suggestions withAnimation:YES];
}

@end

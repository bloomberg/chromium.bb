// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_suggestion_label.h"
#import "ios/chrome/browser/autofill/form_suggestion_view_client.h"
#include "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Vertical margin between suggestions and the edge of the suggestion content
// frame.
const CGFloat kSuggestionVerticalMargin = 6;

// Horizontal margin around suggestions (i.e. between suggestions, and between
// the end suggestions and the suggestion content frame).
const CGFloat kSuggestionHorizontalMargin = 6;

}  // namespace

@implementation FormSuggestionView {
  // The FormSuggestions that are displayed by this view.
  NSArray* _suggestions;
}

- (instancetype)initWithFrame:(CGRect)frame
                       client:(id<FormSuggestionViewClient>)client
                  suggestions:(NSArray*)suggestions {
  self = [super initWithFrame:frame];
  if (self) {
    _suggestions = [suggestions copy];

    self.showsVerticalScrollIndicator = NO;
    self.showsHorizontalScrollIndicator = NO;
    self.bounces = NO;
    self.canCancelContentTouches = YES;

    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.layoutMarginsRelativeArrangement = YES;
    stackView.layoutMargins = UIEdgeInsetsMake(
        kSuggestionVerticalMargin, kSuggestionHorizontalMargin,
        kSuggestionVerticalMargin, kSuggestionHorizontalMargin);
    stackView.spacing = kSuggestionHorizontalMargin;
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:stackView];

    void (^setupBlock)(FormSuggestion* suggestion, NSUInteger idx, BOOL* stop) =
        ^(FormSuggestion* suggestion, NSUInteger idx, BOOL* stop) {
          BOOL userInteractionEnabled =
              suggestion.identifier !=
              autofill::POPUP_ITEM_ID_GOOGLE_PAY_BRANDING;
          UIView* label = [[FormSuggestionLabel alloc]
                  initWithSuggestion:suggestion
                               index:idx
              userInteractionEnabled:userInteractionEnabled
                      numSuggestions:[_suggestions count]
                              client:client];
          [stackView addArrangedSubview:label];
        };
    [_suggestions enumerateObjectsUsingBlock:setupBlock];
    AddSameConstraints(stackView, self);
    [stackView.heightAnchor constraintEqualToAnchor:self.heightAnchor].active =
        true;
  }
  return self;
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  FormSuggestion* firstSuggestion = [_suggestions firstObject];
  if (firstSuggestion.identifier ==
      autofill::POPUP_ITEM_ID_GOOGLE_PAY_BRANDING) {
    UIView* firstLabel = [self.subviews firstObject];
    DCHECK(firstLabel);
    const CGFloat firstLabelWidth =
        CGRectGetWidth([firstLabel frame]) + kSuggestionHorizontalMargin;
    dispatch_time_t popTime =
        dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC);
    __weak FormSuggestionView* weakSelf = self;
    dispatch_after(popTime, dispatch_get_main_queue(), ^{
      [weakSelf setContentOffset:CGPointMake(firstLabelWidth,
                                             weakSelf.contentOffset.y)
                        animated:YES];
    });
  }
  [super willMoveToSuperview:newSuperview];
}

- (NSArray*)suggestions {
  return _suggestions;
}

@end

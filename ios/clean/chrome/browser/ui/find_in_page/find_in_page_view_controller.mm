// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_view_controller.h"

#include "base/format_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_view.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// For the first |kSearchDelayChars| characters, delay by |kSearchLongDelay|
// For the remaining characters, delay by |kSearchShortDelay|.
const NSUInteger kSearchDelayChars = 3;
const NSTimeInterval kSearchLongDelay = 1.0;
const NSTimeInterval kSearchShortDelay = 0.100;
}  // namespace

@interface FindInPageViewController ()
// Redeclare the |view| property to be the FindBarView subclass instead of a
// generic UIView.
@property(nonatomic, readwrite, strong) FindBarView* view;

// Typing delay timer.
@property(nonatomic, strong) NSTimer* delayTimer;
@end

@implementation FindInPageViewController

@dynamic view;
@synthesize delayTimer = _delayTimer;
@synthesize dispatcher = _dispatcher;

- (void)loadView {
  self.view = [[FindBarView alloc] initWithDarkAppearance:NO];
}

- (void)viewDidLoad {
  FindBarView* findBarView = self.view;
  findBarView.backgroundColor = [UIColor whiteColor];
  [findBarView.inputField addTarget:self
                             action:@selector(findBarTextDidChange)
                   forControlEvents:UIControlEventEditingChanged];
  [findBarView.nextButton addTarget:self
                             action:@selector(findNextInPage)
                   forControlEvents:UIControlEventTouchUpInside];
  [findBarView.previousButton addTarget:self
                                 action:@selector(findPreviousInPage)
                       forControlEvents:UIControlEventTouchUpInside];
  [findBarView.closeButton addTarget:self
                              action:@selector(hideFindInPage)
                    forControlEvents:UIControlEventTouchUpInside];
}

- (void)setCurrentMatch:(NSUInteger)current ofTotalMatches:(NSUInteger)total {
  NSString* indexStr = [NSString stringWithFormat:@"%" PRIdNS, current];
  NSString* matchesStr = [NSString stringWithFormat:@"%" PRIdNS, total];
  NSString* text = l10n_util::GetNSStringF(
      IDS_FIND_IN_PAGE_COUNT, base::SysNSStringToUTF16(indexStr),
      base::SysNSStringToUTF16(matchesStr));
  [static_cast<FindBarView*>(self.view) updateResultsLabelWithText:text];
}

- (void)textChanged {
  [self.dispatcher findStringInPage:self.view.inputField.text];
}

#pragma mark - Actions

- (void)showFindInPage {
  [self.dispatcher showFindInPage];
}

- (void)findNextInPage {
  [self.dispatcher findNextInPage];
}

- (void)findPreviousInPage {
  [self.dispatcher findPreviousInPage];
}

- (void)hideFindInPage {
  [self.dispatcher hideFindInPage];
}

- (void)findBarTextDidChange {
  [self.delayTimer invalidate];
  NSUInteger length = [self.view.inputField.text length];
  if (length == 0)
    return [self textChanged];

  // Delay delivery of text change event.  Use a longer delay when the input
  // length is short.
  NSTimeInterval delay =
      (length > kSearchDelayChars) ? kSearchShortDelay : kSearchLongDelay;
  self.delayTimer =
      [NSTimer scheduledTimerWithTimeInterval:delay
                                       target:self
                                     selector:@selector(textChanged)
                                     userInfo:nil
                                      repeats:NO];
}

@end

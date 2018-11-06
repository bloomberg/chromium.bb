// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sad_tab/sad_tab_view_controller.h"

#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SadTabViewController ()<SadTabViewDelegate>

@property(nonatomic) SadTabView* sadTabView;

@end

@implementation SadTabViewController

@synthesize repeatedFailure = _repeatedFailure;
@synthesize offTheRecord = _offTheRecord;
@synthesize sadTabView = _sadTabView;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  SadTabViewMode mode =
      self.repeatedFailure ? SadTabViewMode::FEEDBACK : SadTabViewMode::RELOAD;
  self.sadTabView =
      [[SadTabView alloc] initWithMode:mode offTheRecord:self.offTheRecord];
  self.sadTabView.translatesAutoresizingMaskIntoConstraints = NO;
  self.sadTabView.delegate = self;
  [self.view addSubview:self.sadTabView];

  AddSameConstraints(self.sadTabView, self.view);
}

#pragma mark - SadTabViewDelegate

- (void)sadTabViewShowReportAnIssue:(SadTabView*)sadTabView {
  [self.delegate sadTabViewControllerShowReportAnIssue:self];
}

- (void)sadTabView:(SadTabView*)sadTabView
    showSuggestionsPageWithURL:(const GURL&)URL {
  [self.delegate sadTabViewController:self showSuggestionsPageWithURL:URL];
}

- (void)sadTabViewReload:(SadTabView*)sadTabView {
  [self.delegate sadTabViewControllerReload:self];
}

@end

#pragma mark -

@implementation SadTabViewController (UIElements)

- (UITextView*)messageTextView {
  return self.sadTabView.messageTextView;
}

- (UIButton*)actionButton {
  return self.sadTabView.actionButton;
}

@end

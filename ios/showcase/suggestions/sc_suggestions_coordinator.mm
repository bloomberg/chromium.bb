// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/suggestions/sc_suggestions_coordinator.h"

#import "ios/chrome/browser/ui/suggestions/suggestions_commands.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCSuggestionsCoordinator ()<SuggestionsCommands>

@property(nonatomic, strong)
    SuggestionsViewController* suggestionViewController;

@end

@implementation SCSuggestionsCoordinator

@synthesize baseViewController;
@synthesize suggestionViewController = _suggestionViewController;

#pragma mark - Coordinator

- (void)start {
  _suggestionViewController = [[SuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault];

  _suggestionViewController.suggestionCommandHandler = self;

  [self.baseViewController pushViewController:_suggestionViewController
                                     animated:YES];
}

#pragma mark - SuggestionsCommands

- (void)addEmptyItem {
  [self.suggestionViewController addTextItem:@"Button clicked"
                                    subtitle:@"Item Added!"
                                   toSection:5];
}

@end

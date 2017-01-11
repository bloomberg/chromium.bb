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

@end

@implementation SCSuggestionsCoordinator

@synthesize baseViewController;

#pragma mark - Coordinator

- (void)start {
  SuggestionsViewController* suggestion = [[SuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault];

  suggestion.suggestionCommandHandler = self;

  [self.baseViewController pushViewController:suggestion animated:YES];
}

#pragma mark - SuggestionsCommands

- (void)addEmptyItem {
}

@end

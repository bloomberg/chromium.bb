// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/suggestions/sc_suggestions_coordinator.h"

#import "ios/chrome/browser/ui/suggestions/suggestions_commands.h"
#import "ios/chrome/browser/ui/suggestions/suggestions_view_controller.h"
#import "ios/showcase/common/protocol_alerter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCSuggestionsCoordinator ()

@property(nonatomic, strong)
    SuggestionsViewController* suggestionViewController;
@property(nonatomic, strong) ProtocolAlerter* alerter;

@end

@implementation SCSuggestionsCoordinator

@synthesize baseViewController;
@synthesize suggestionViewController = _suggestionViewController;
@synthesize alerter = _alerter;

#pragma mark - Coordinator

- (void)start {
  self.alerter = [[ProtocolAlerter alloc]
      initWithProtocols:@[ @protocol(SuggestionsCommands) ]];
  self.alerter.baseViewController = self.baseViewController;

  _suggestionViewController = [[SuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault];

  _suggestionViewController.suggestionCommandHandler =
      reinterpret_cast<id<SuggestionsCommands>>(self.alerter);

  [self.baseViewController pushViewController:_suggestionViewController
                                     animated:YES];
}

@end

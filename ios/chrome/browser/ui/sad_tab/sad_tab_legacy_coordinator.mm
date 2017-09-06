// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sad_tab/sad_tab_legacy_coordinator.h"

#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"
#include "ios/web/public/web_state/web_state.h"

@implementation SadTabLegacyCoordinator
@synthesize dispatcher = _dispatcher;
@synthesize webState = _webState;

#pragma mark - SadTabTabHelperDelegate

- (void)presentSadTabForRepeatedFailure:(BOOL)repeatedFailure {
  // Create a SadTabView so |webstate| presents it.
  SadTabView* sadTabview = [[SadTabView alloc]
           initWithMode:repeatedFailure ? SadTabViewMode::FEEDBACK
                                        : SadTabViewMode::RELOAD
      navigationManager:self.webState->GetNavigationManager()];
  sadTabview.dispatcher = static_cast<id<ApplicationCommands>>(self.dispatcher);
  CRWContentView* contentView =
      [[CRWGenericContentView alloc] initWithView:sadTabview];
  self.webState->ShowTransientContentView(contentView);
}

@end

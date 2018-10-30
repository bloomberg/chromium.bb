// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sad_tab/sad_tab_legacy_coordinator.h"

#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"
#import "ios/web/public/web_state/web_state.h"

@interface SadTabLegacyCoordinator ()<SadTabViewDelegate>
@end

@implementation SadTabLegacyCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize dispatcher = _dispatcher;

#pragma mark - SadTabViewDelegate

- (void)sadTabViewShowReportAnIssue:(SadTabView*)sadTabView {
  [self.dispatcher showReportAnIssueFromViewController:self.baseViewController];
}

- (void)sadTabView:(SadTabView*)sadTabView
    showSuggestionsPageWithURL:(const GURL&)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher openURLInNewTab:command];
}

- (void)sadTabViewReload:(SadTabView*)sadTabView {
  [self.dispatcher reload];
}

#pragma mark - SadTabTabHelperDelegate

- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    presentSadTabForWebState:(web::WebState*)webState
             repeatedFailure:(BOOL)repeatedFailure {
  SadTabViewMode mode =
      repeatedFailure ? SadTabViewMode::FEEDBACK : SadTabViewMode::RELOAD;
  SadTabView* sadTabView = [[SadTabView alloc]
      initWithMode:mode
      offTheRecord:webState->GetBrowserState()->IsOffTheRecord()];
  sadTabView.delegate = self;
  CRWContentView* contentView =
      [[CRWGenericContentView alloc] initWithView:sadTabView];
  webState->ShowTransientContentView(contentView);
}

@end

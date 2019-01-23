// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_url_loader.h"

#include "components/sessions/core/session_types.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sessions/session_util.h"
#include "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Appends and activates |web_state| to the end of |web_state_list|.
void AppendAndActivateWebState(WebStateList* web_state_list,
                               std::unique_ptr<web::WebState> web_state) {
  web_state_list->InsertWebState(
      web_state_list->count(), std::move(web_state),
      (WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE),
      WebStateOpener());
}
}  // namespace

@interface TabGridURLLoader ()
@property(nonatomic, assign) WebStateList* regularWebStateList;
@property(nonatomic, assign) ios::ChromeBrowserState* regularBrowserState;
@end

@implementation TabGridURLLoader

- (instancetype)
initWithRegularWebStateList:(WebStateList*)regularWebStateList
      incognitoWebStateList:(WebStateList*)incognitoWebStateList
        regularBrowserState:(ios::ChromeBrowserState*)regularBrowserState
      incognitoBrowserState:(ios::ChromeBrowserState*)incognitoBrowserState {
  if (self = [super init]) {
    _regularWebStateList = regularWebStateList;
    _regularBrowserState = regularBrowserState;
    _incognitoWebStateList = incognitoWebStateList;
    _incognitoBrowserState = incognitoBrowserState;
  }
  return self;
}

#pragma mark - UrlLoader

// In tab grid, |inBackground| is ignored, which means that the new WebState is
// activated. |appendTo| is also ignored, so the new WebState is always appended
// at the end of the list. The page transition type is explicit rather than
// linked.
- (void)webPageOrderedOpen:(OpenNewTabCommand*)command {
  WebStateList* webStateList;
  ios::ChromeBrowserState* browserState;
  if (command.inIncognito) {
    webStateList = self.incognitoWebStateList;
    browserState = self.incognitoBrowserState;
  } else {
    webStateList = self.regularWebStateList;
    browserState = self.regularBrowserState;
  }
  DCHECK(webStateList);
  DCHECK(browserState);
  web::WebState::CreateParams params(browserState);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
  web::NavigationManager::WebLoadParams loadParams(command.URL);
  loadParams.referrer = command.referrer;
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);
  AppendAndActivateWebState(webStateList, std::move(webState));
}

// Opens |loadParams| in a new regular tab, rather than replacing the active
// tab.
- (void)loadURLWithParams:(const ChromeLoadParams&)loadParams {
  DCHECK(self.regularBrowserState);
  web::WebState::CreateParams params(self.regularBrowserState);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
  webState->GetNavigationManager()->LoadURLWithParams(loadParams.web_params);
  AppendAndActivateWebState(self.regularWebStateList, std::move(webState));
}

@end

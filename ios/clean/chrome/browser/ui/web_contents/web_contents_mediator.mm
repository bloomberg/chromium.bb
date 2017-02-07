// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_contents_mediator.h"

#import "ios/clean/chrome/browser/ui/web_contents/web_contents_consumer.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebContentsMediator
@synthesize webState = _webState;
@synthesize consumer = _consumer;

- (void)setWebState:(web::WebState*)webState {
  if (_webState)
    _webState->SetWebUsageEnabled(false);

  _webState = webState;
  if (!self.webState)
    return;

  self.webState->SetWebUsageEnabled(true);
  if (!self.webState->GetNavigationManager()->GetItemCount()) {
    web::NavigationManager::WebLoadParams params(
        GURL("https://dev.chromium.org/"));
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    self.webState->GetNavigationManager()->LoadURLWithParams(params);
  }
  [self.consumer contentViewDidChange:self.webState->GetView()];
}

- (void)setConsumer:(id<WebContentsConsumer>)consumer {
  _consumer = consumer;
  if (self.webState)
    [self.consumer contentViewDidChange:self.webState->GetView()];
}

@end

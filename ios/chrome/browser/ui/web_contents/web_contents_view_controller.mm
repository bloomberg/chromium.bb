// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/chrome/browser/ui/web_contents/web_contents_view_controller.h"

#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebContentsViewController

@synthesize webState = _webState;

- (instancetype)initWithWebState:(web::WebState*)webState {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    DCHECK(webState);
    _webState = webState;
  }
  return self;
}

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor colorWithWhite:0.75 alpha:1.0];

  UIView* webContentsView = self.webState->GetView();
  webContentsView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:webContentsView];
  [NSLayoutConstraint activateConstraints:@[
    [webContentsView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [webContentsView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [webContentsView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [webContentsView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  if (!self.webState->GetNavigationManager()->GetItemCount()) {
    web::NavigationManager::WebLoadParams params(
        GURL("https://dev.chromium.org/"));
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    _webState->GetNavigationManager()->LoadURLWithParams(params);
  }
}

@end

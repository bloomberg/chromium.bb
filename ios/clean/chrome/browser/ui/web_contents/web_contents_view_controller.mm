// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/web_contents/web_contents_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebContentsViewController ()
@property(nonatomic, strong) UIView* contentView;
@end

@implementation WebContentsViewController

@synthesize contentView = _contentView;

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor colorWithWhite:0.75 alpha:1.0];

  [self updateContentView];
}

#pragma mark - WebContentsConsumer

- (void)contentViewDidChange:(UIView*)contentView {
  if (contentView == self.contentView)
    return;

  // If there was a previous content view, remove it from the view hierarchy.
  [self.contentView removeFromSuperview];
  self.contentView = contentView;

  // If self.view hasn't loaded yet, this call shouldn't induce that load.
  // (calling self.view will trigger -loadView, etc.). Only update for the
  // new content view if there's a view to update.
  if (self.viewIfLoaded)
    [self updateContentView];
}

#pragma mark - Private methods

- (void)updateContentView {
  self.contentView.frame = self.view.bounds;
  self.contentView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:self.contentView];
}

@end

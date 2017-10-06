// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container_view.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrowserContainerView {
  // Weak reference to content view, so old _contentView can be removed from
  // superview when new one is added.
  __weak UIView* _contentView;
}

- (void)dealloc {
  DCHECK(![_contentView superview] || [_contentView superview] == self);
}

- (void)displayContentView:(UIView*)contentView {
  DCHECK(![_contentView superview] || [_contentView superview] == self);
  [_contentView removeFromSuperview];
  _contentView = contentView;

  if (contentView) {
    [self addSubview:contentView];
  }
}

@end

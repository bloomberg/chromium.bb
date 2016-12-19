// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container_view.h"

#import "base/ios/weak_nsobject.h"

@implementation BrowserContainerView {
  // Weak reference to content view, so old _contentView can be removed from
  // superview when new one is added.
  base::WeakNSObject<UIView> _contentView;
}

- (void)dealloc {
  DCHECK(![_contentView superview] || [_contentView superview] == self);

  [super dealloc];
}

- (void)displayContentView:(UIView*)contentView {
  DCHECK(![_contentView superview] || [_contentView superview] == self);
  [_contentView removeFromSuperview];
  _contentView.reset(contentView);

  if (contentView) {
    [self addSubview:contentView];
  }
}

@end

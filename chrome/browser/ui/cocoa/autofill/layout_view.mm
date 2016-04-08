// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/layout_view.h"

#include <utility>

#include "chrome/browser/ui/cocoa/autofill/simple_grid_layout.h"

@implementation LayoutView

- (void)setLayoutManager:(std::unique_ptr<SimpleGridLayout>)layout {
  layout_ = std::move(layout);
}

- (SimpleGridLayout*)layoutManager {
  return layout_.get();
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize {
  [super resizeSubviewsWithOldSize:oldBoundsSize];
  [self performLayout];
}

- (void)performLayout {
  layout_->Layout(self);
}

- (CGFloat)preferredHeightForWidth:(CGFloat)width {
  return layout_->GetPreferredHeightForWidth(width);
}

@end

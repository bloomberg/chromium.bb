// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hover_image_button.h"

#include "base/scoped_nsobject.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

@implementation HoverImageButton

- (void)drawRect:(NSRect)rect {
  if (hoverState_ == kHoverStateMouseDown && pressedImage_) {
    [super setImage:pressedImage_.get()];
    [super setAlphaValue:pressedOpacity_];
  } else if (hoverState_ == kHoverStateMouseOver && hoverImage_) {
    [super setImage:hoverImage_.get()];
    [super setAlphaValue:hoverOpacity_];
  } else {
    [super setImage:defaultImage_.get()];
    [super setAlphaValue:defaultOpacity_];
  }

  [super drawRect:rect];
}

- (void)setDefaultImage:(NSImage*)image {
  defaultImage_.reset([image retain]);
}

- (void)setDefaultOpacity:(float)opacity {
  defaultOpacity_ = opacity;
}

- (void)setHoverImage:(NSImage*)image {
  hoverImage_.reset([image retain]);
}

- (void)setHoverOpacity:(float)opacity {
  hoverOpacity_ = opacity;
}

- (void)setPressedImage:(NSImage*)image {
  pressedImage_.reset([image retain]);
}

- (void)setPressedOpacity:(float)opacity {
  pressedOpacity_ = opacity;
}

@end

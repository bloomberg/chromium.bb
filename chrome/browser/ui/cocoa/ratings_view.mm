// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/ratings_view.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "ui/base/resource/resource_bundle.h"

@implementation RatingsView2

- (id)initWithRating:(double)rating {
  if ((self = [super initWithFrame:NSZeroRect])) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    NSRect viewFrame = NSZeroRect;
    for (int i = 0; i < 5; ++i) {
      scoped_nsobject<NSImageView> imageView([[NSImageView alloc] init]);
      [imageView setImageFrameStyle:NSImageFrameNone];
      int imageID = WebIntentPicker::GetNthStarImageIdFromCWSRating(rating, i);
      NSImage* image = rb.GetNativeImageNamed(imageID).ToNSImage();
      [imageView setImage:image];

      NSRect imageFrame;
      imageFrame.size = [image size];
      imageFrame.origin.x = NSMaxX(viewFrame);
      imageFrame.origin.y = 0;
      [imageView setFrame:imageFrame];
      viewFrame.size.width = NSMaxX(imageFrame);
      viewFrame.size.height =
          std::max(viewFrame.size.height, NSHeight(imageFrame));

      [self addSubview:imageView];
    }
    [self setFrame:viewFrame];
  }
  return self;
}

@end

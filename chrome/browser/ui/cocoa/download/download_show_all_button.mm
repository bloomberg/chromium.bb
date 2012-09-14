// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_show_all_button.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/download/download_show_all_cell.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

@implementation DownloadShowAllButton

- (void)awakeFromNib {
  DCHECK([[self cell] isKindOfClass:[DownloadShowAllCell class]]);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* favicon = rb.GetNativeImageNamed(IDR_DOWNLOADS_FAVICON).ToNSImage();
  [self setImage:favicon];
}

// GTM's layout tweaker calls sizeToFit to receive the desired width of views.
// By default, buttons will be only 14px high, but the Show All button needs to
// be higher.
- (void)sizeToFit {
  NSRect oldRect = [self frame];
  [super sizeToFit];
  NSRect newRect = [self frame];

  // Keep old height.
  newRect.origin.y = oldRect.origin.y;
  newRect.size.height = oldRect.size.height;

  [self setFrame:newRect];
}

@end

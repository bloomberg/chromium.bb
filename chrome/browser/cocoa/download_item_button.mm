// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_item_button.h"

#include "base/sys_string_conversions.h"

@implementation DownloadItemButton

@synthesize download = downloadPath_;

// Overridden from DraggableButton.
- (void)beginDrag:(NSEvent*)event {
  if (!downloadPath_.empty()) {
    NSString* filename = base::SysUTF8ToNSString(downloadPath_.value());
    [self dragFile:filename fromRect:[self bounds] slideBack:YES event:event];
  }
}

@end

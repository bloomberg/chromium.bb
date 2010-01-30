// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#import "chrome/browser/cocoa/draggable_button.h"

// A button that is a drag source for a file.
@interface DownloadItemButton : DraggableButton {
 @private
  FilePath downloadPath_;
}

@property(assign, nonatomic) FilePath download;

// Overridden from DraggableButton.
- (void)beginDrag:(NSEvent*)event;

@end

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/file_path.h"
#import "chrome/browser/ui/cocoa/draggable_button.h"

@class DownloadItemController;

// A button that is a drag source for a file and that displays a context menu
// instead of firing an action when clicked in a certain area.
@interface DownloadItemButton : DraggableButton<NSMenuDelegate> {
 @private
  FilePath downloadPath_;
  DownloadItemController* controller_;  // weak
}

@property(assign, nonatomic) FilePath download;
@property(assign, nonatomic) DownloadItemController* controller;

// Overridden from DraggableButton.
- (void)beginDrag:(NSEvent*)event;

@end

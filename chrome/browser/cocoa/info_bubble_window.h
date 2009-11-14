// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// A rounded window with an arrow used for example when you click on the STAR
// button or that pops up within our first-run UI.
@interface InfoBubbleWindow : NSWindow {
 @private
  // Is self in the process of closing.
  BOOL closing_;
}
@end

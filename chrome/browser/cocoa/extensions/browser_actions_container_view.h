// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@interface BrowserActionsContainerView : NSView {
  // Whether there is a border to the right of the last Browser Action.
  BOOL rightBorderShown_;
}

@property(nonatomic) BOOL rightBorderShown;

@end

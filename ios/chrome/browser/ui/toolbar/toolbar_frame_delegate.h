// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_FRAME_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_FRAME_DELEGATE_H_

#import <Foundation/Foundation.h>

// Create a thin wrapper around UIImageView to catch frame change and window
// events.
@protocol ToolbarFrameDelegate<NSObject>
- (void)frameDidChangeFrame:(CGRect)newFrame fromFrame:(CGRect)oldFrame;
- (void)windowDidChange;
- (void)traitCollectionDidChange;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_FRAME_DELEGATE_H_

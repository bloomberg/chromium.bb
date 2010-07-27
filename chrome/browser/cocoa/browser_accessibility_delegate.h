// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_DELEGATE_H
#define CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_DELEGATE_H
#pragma once

@class BrowserAccessibility;
@class NSWindow;

// This protocol is used by the BrowserAccessibility objects to pass messages
// to, or otherwise communicate with, their underlying WebAccessibility
// objects over the IPC boundary.
@protocol BrowserAccessibilityDelegate
- (NSPoint)accessibilityPointInScreen:(BrowserAccessibility*)accessibility;
- (void)doDefaultAction:(int32)accessibilityObjectId;
- (void)setAccessibilityFocus:(BOOL)focus
              accessibilityId:(int32)accessibilityObjectId;
- (NSWindow*)window;
@end

#endif // CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_DELEGATE_H

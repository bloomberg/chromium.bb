// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_DELEGATE_H
#define CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_DELEGATE_H

@class BrowserAccessibility;
@class NSWindow;

@protocol BrowserAccessibilityDelegate
- (NSPoint)accessibilityPointInScreen:(BrowserAccessibility*)accessibility;
- (void)doDefaultAction:(int32)accessibilityObjectId;
- (void)setAccessibilityFocus:(BOOL)focus
              accessibilityId:(int32)accessibilityObjectId;
- (NSWindow*)window;
@end

#endif // CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_DELEGATE_H

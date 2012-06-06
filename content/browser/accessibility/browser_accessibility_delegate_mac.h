// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_DELEGATE_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_DELEGATE_MAC_H_
#pragma once

@class BrowserAccessibilityCocoa;
@class NSWindow;

// This protocol is used by the BrowserAccessibility objects to pass messages
// to, or otherwise communicate with, their underlying WebAccessibility
// objects over the IPC boundary.
@protocol BrowserAccessibilityDelegateCocoa
- (NSPoint)accessibilityPointInScreen:(BrowserAccessibilityCocoa*)accessibility;
- (void)doDefaultAction:(int32)accessibilityObjectId;
- (void)accessibilitySetTextSelection:(int32)accId
                          startOffset:(int32)startOffset
                            endOffset:(int32)endOffset;
- (void)performShowMenuAction:(BrowserAccessibilityCocoa*)accessibility;
- (void)setAccessibilityFocus:(BOOL)focus
              accessibilityId:(int32)accessibilityObjectId;
- (NSWindow*)window;
@end

#endif // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_DELEGATE_MAC_H_

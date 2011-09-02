// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// HyperlinkTextView is an NSTextView subclass for unselectable, linkable text.
// This subclass doesn't show the text caret or IBeamCursor, whereas the base
// class NSTextView displays both with full keyboard accessibility enabled.
@interface HyperlinkTextView : NSTextView
// Convenience function that sets the |HyperlinkTextView| contents to the
// specified |message| with a hypertext style |link| inserted at |linkOffset|.
// Uses the supplied |font|, |messageColor|, and |linkColor|.
- (void)setMessageAndLink:(NSString*)message
                 withLink:(NSString*)link
                 atOffset:(NSUInteger)linkOffset
                     font:(NSFont*)font
             messageColor:(NSColor*)messageColor
                linkColor:(NSColor*)linkColor;
@end

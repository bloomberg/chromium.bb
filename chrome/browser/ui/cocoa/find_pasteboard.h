// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FIND_PASTEBOARD_H_
#define CHROME_BROWSER_UI_COCOA_FIND_PASTEBOARD_H_
#pragma once

#include "base/string16.h"

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

extern NSString* kFindPasteboardChangedNotification;

// Manages the find pasteboard. Use this to copy text to the find pasteboard,
// to get the text currently on the find pasteboard, and to receive
// notifications when the text on the find pasteboard has changed. You should
// always use this class instead of accessing
// [NSPasteboard pasteboardWithName:NSFindPboard] directly.
//
// This is not thread-safe and must be used on the main thread.
//
// This is supposed to be a singleton.
@interface FindPasteboard : NSObject {
 @private
  scoped_nsobject<NSString> findText_;
}

// Returns the singleton instance of this class.
+ (FindPasteboard*)sharedInstance;

// Returns the current find text. This is never nil; if there is no text on the
// find pasteboard, this returns an empty string.
- (NSString*)findText;

// Sets the current find text to |newText| and sends a
// |kFindPasteboardChangedNotification| to the default notification center if
// it the new text different from the current text. |newText| must not be nil.
- (void)setFindText:(NSString*)newText;
@end

@interface FindPasteboard (TestingAPI)
- (void)loadTextFromPasteboard:(NSNotification*)notification;

// This methods is meant to be overridden in tests.
- (NSPasteboard*)findPboard;
@end

#endif  // __OBJC__

// Also provide a c++ interface
string16 GetFindPboardText();

#endif  // CHROME_BROWSER_UI_COCOA_FIND_PASTEBOARD_H_

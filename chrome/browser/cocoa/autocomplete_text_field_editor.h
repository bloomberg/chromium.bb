// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// Field editor used for the autocomplete field.
@interface AutocompleteTextFieldEditor : NSTextView {
}

// Copy contents of the TextView to the designated clipboard as plain
// text.
- (void)performCopy:(NSPasteboard*)pb;

// Same as above, note that this calls through to performCopy.
- (void)performCut:(NSPasteboard*)pb;
@end

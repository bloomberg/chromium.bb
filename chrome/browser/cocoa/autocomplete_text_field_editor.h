// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@protocol AutocompleteTextFieldEditorDelegateMethods

// Delegate -paste: implementation to the field being edited.  If the
// delegate returns YES, or does not implement the method, NSTextView
// is called to handle the paste.  The delegate can block the paste
// (or handle it internally) by returning NO.
- (BOOL)textShouldPaste:(NSText*)fieldEditor;

@end

// Field editor used for the autocomplete field.
@interface AutocompleteTextFieldEditor : NSTextView {
}

// Copy contents of the TextView to the designated clipboard as plain
// text.
- (void)performCopy:(NSPasteboard*)pb;

// Same as above, note that this calls through to performCopy.
- (void)performCut:(NSPasteboard*)pb;

// Called by -paste: to decide whether to forward to superclass.
// Exposed for unit testing.
- (BOOL)shouldPaste;

@end

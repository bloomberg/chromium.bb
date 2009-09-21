// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

class AutocompleteTextFieldObserver;

// AutocompleteTextFieldEditor customized the AutocompletTextField
// field editor (helper text-view used in editing).  It intercepts UI
// events for forwarding to the core Omnibox code.  It also undoes
// some of the effects of using styled text in the Omnibox (the text
// is styled but should not appear that way when copied to the
// pasteboard).

// TODO(shess): Move delegate stuff to AutocompleteTextFieldObserver.

@protocol AutocompleteTextFieldEditorDelegateMethods

// Returns nil if paste actions are not supported.
- (NSString*)textPasteActionString:(NSText*)fieldEditor;
- (void)textDidPasteAndGo:(NSText*)fieldEditor;
@end

// Field editor used for the autocomplete field.
@interface AutocompleteTextFieldEditor : NSTextView {
}

// Copy contents of the TextView to the designated clipboard as plain
// text.
- (void)performCopy:(NSPasteboard*)pb;

// Same as above, note that this calls through to performCopy.
- (void)performCut:(NSPasteboard*)pb;

@end

@interface AutocompleteTextFieldEditor(PrivateTestMethods)
- (AutocompleteTextFieldObserver*)observer;
@end

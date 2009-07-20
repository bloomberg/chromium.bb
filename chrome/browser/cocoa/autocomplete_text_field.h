// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_
#define CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_

#import <Cocoa/Cocoa.h>

// TODO(shess): This class will add decorations to support keyword
// search and hints.  Adding as a stub so that I can clean up naming
// around this code all at once before layering other changes over in
// parallel.

@protocol AutocompleteTextFieldDelegateMethods

// Delegate -textShouldPaste: implementation to the field being
// edited.  See AutocompleteTextFieldEditor implementation.
- (BOOL)control:(NSControl*)control textShouldPaste:(NSText*)fieldEditor;

@end

@interface AutocompleteTextField : NSTextField {
}

- (BOOL)textShouldPaste:(NSText*)fieldEditor;

@end

#endif  // CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_
#define CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_

#import <Cocoa/Cocoa.h>

@class AutocompleteTextFieldCell;

// AutocompleteTextField intercepts UI actions for forwarding to
// AutocompleteEditViewMac (*), and provides a custom look.  It works
// together with AutocompleteTextFieldEditor (mostly for intercepting
// user actions) and AutocompleteTextFieldCell (mostly for custom
// drawing).
//
// For historical reasons, chrome/browser/autocomplete is the core
// implementation of the Omnibox.  Chrome code seems to vary between
// autocomplete and Omnibox in describing this.
//
// (*) AutocompleteEditViewMac is a view in the MVC sense for the
// Chrome internals, though it's really more of a mish-mash of model,
// view, and controller.

// AutocompleteTextFieldDelegateMethods are meant to be similar to
// NSControl delegate methods, adding additional intercepts relevant
// to the Omnibox implementation.

@protocol AutocompleteTextFieldDelegateMethods

// Delegate -textShouldPaste: implementation to the field being
// edited.  See AutocompleteTextFieldEditor implementation.
- (BOOL)control:(NSControl*)control textShouldPaste:(NSText*)fieldEditor;

// Let the delegate track -flagsChanged: events.
- (void)control:(NSControl*)control flagsChanged:(NSEvent*)theEvent;

@end

@interface AutocompleteTextField : NSTextField {
}

- (BOOL)textShouldPaste:(NSText*)fieldEditor;

// Convenience method to return the cell, casted appropriately.
- (AutocompleteTextFieldCell*)autocompleteTextFieldCell;

// If the keyword, keyword hint, or search hint changed, then the
// field needs to be relaidout.  This accomplishes that in a manner
// which doesn't disrupt the field delegate.
- (void)resetFieldEditorFrameIfNeeded;

@end

#endif  // CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_

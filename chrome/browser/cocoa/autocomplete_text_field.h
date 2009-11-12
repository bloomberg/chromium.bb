// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_
#define CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/styled_text_field.h"

#include "base/scoped_nsobject.h"

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

// Provides a hook so that we can call directly down to
// AutocompleteEditViewMac rather than traversing the delegate chain.
class AutocompleteTextFieldObserver {
 public:
  // Called when the control-key state changes while the field is
  // first responder.
  virtual void OnControlKeyChanged(bool pressed) = 0;

  // Called when the user pastes into the field.
  virtual void OnPaste() = 0;

  // Returns true if the current clipboard text supports paste and go
  // (or paste and search).
  virtual bool CanPasteAndGo() = 0;

  // Returns the appropriate "Paste and Go" or "Paste and Search"
  // context menu string, depending on what is currently in the
  // clipboard.  Must not be called unless CanPasteAndGo() returns
  // true.
  virtual int GetPasteActionStringId() = 0;

  // Called when the user initiates a "paste and go" or "paste and
  // search" into |field_|.
  virtual void OnPasteAndGo() = 0;

  // Called when the field's frame changes.
  virtual void OnFrameChanged() = 0;
};

@interface AutocompleteTextField : StyledTextField {
 @private
  // Undo manager for this text field.  We use a specific instance rather than
  // the standard undo manager in order to let us clear the undo stack at will.
  scoped_nsobject<NSUndoManager> undoManager_;

  AutocompleteTextFieldObserver* observer_;  // weak, owned by location bar.
}

@property AutocompleteTextFieldObserver* observer;

// Convenience method to return the cell, casted appropriately.
- (AutocompleteTextFieldCell*)autocompleteTextFieldCell;

// Superclass aborts editing before changing the string, which causes
// problems for undo.  This version modifies the field editor's
// contents if the control is already being edited.
- (void)setAttributedStringValue:(NSAttributedString*)aString;

// Clears the undo chain for this text field.
- (void)clearUndoChain;

@end

#endif  // CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_H_

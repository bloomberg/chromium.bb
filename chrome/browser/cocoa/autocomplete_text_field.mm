// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/autocomplete_text_field.h"

#include "base/logging.h"
#include "chrome/browser/cocoa/autocomplete_text_field_cell.h"

@implementation AutocompleteTextField

+ (Class)cellClass {
  return [AutocompleteTextFieldCell class];
}

- (void)awakeFromNib {
  DCHECK([[self cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
}

- (BOOL)textShouldPaste:(NSText*)fieldEditor {
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(control:textShouldPaste:)]) {
    return [delegate control:self textShouldPaste:fieldEditor];
  }
  return YES;
}

- (void)flagsChanged:(NSEvent*)theEvent {
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(control:flagsChanged:)]) {
    [delegate control:self flagsChanged:theEvent];
  }
  [super flagsChanged:theEvent];
}

- (AutocompleteTextFieldCell*)autocompleteTextFieldCell {
  DCHECK([[self cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
  return static_cast<AutocompleteTextFieldCell*>([self cell]);
}

// TODO(shess): An alternate implementation of this would be to pick
// out the field's subview (which may be a clip view around the field
// editor) and manually resize it to the textFrame returned from the
// cell's -splitFrame:*.  That doesn't mess with the editing state of
// the field editor system, but does make other assumptions about how
// field editors are placed.
- (void)resetFieldEditorFrameIfNeeded {
  AutocompleteTextFieldCell* cell = [self cell];
  if ([cell fieldEditorNeedsReset]) {
    NSTextView* editor = (id)[self currentEditor];
    if (editor) {
      // Clear the delegate so that it doesn't see
      // -control:textShouldEndEditing: (closes autocomplete popup).
      id delegate = [self delegate];
      [self setDelegate:nil];

      // -makeFirstResponder: will select-all, restore selection.
      NSRange sel = [editor selectedRange];
      [[self window] makeFirstResponder:self];
      [editor setSelectedRange:sel];

      [self setDelegate:delegate];

      // Now provoke call to delegate's -controlTextDidBeginEditing:.
      // This is necessary to make sure that we'll send the
      // appropriate -control:textShouldEndEditing: call when we lose
      // focus.
      // TODO(shess): Would be better to only restore this state if
      // -controlTextDidBeginEditing: had already been sent.
      // Unfortunately, that's hard to detect.  Could either check
      // popup IsOpen() or model has_focus()?
      [editor shouldChangeTextInRange:sel replacementString:@""];
    }
    [cell setFieldEditorNeedsReset:NO];
  }
}

// Clicks in the frame outside the field editor will attempt to make
// us first-responder, which will select-all.  So decline
// first-responder when the field editor is active.
- (BOOL)acceptsFirstResponder {
  if ([self currentEditor]) {
    return NO;
  }
  return [super acceptsFirstResponder];
}

// Reroute events for the decoration area to the field editor.  This
// will cause the cursor to be moved as close to the edge where the
// event was seen as possible.
//
// TODO(shess) Check this in light of the -acceptsFirstResponder
// change.  It may no longer be needed.
- (void)mouseDown:(NSEvent *)theEvent {
  NSText* editor = [self currentEditor];
  if (editor) {
    [editor mouseDown:theEvent];
  } else {
    [super mouseDown:theEvent];
  }
}

@end

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_INPUT_FIELD_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_INPUT_FIELD_H_

#import <Cocoa/Cocoa.h>

@protocol AutofillInputField;

// Access to cell state for autofill input controls.
@protocol AutofillInputCell<NSObject>

@property(nonatomic, assign) BOOL invalid;
@property(nonatomic, copy) NSString* fieldValue;
@property(nonatomic, copy) NSString* defaultValue;

@end

// Delegate to handle editing events on the AutofillInputFields.
@protocol AutofillInputDelegate<NSObject>

// Indicates if an event should be forwarded on.
enum KeyEventHandled {
  kKeyEventNotHandled,
  kKeyEventHandled
};

// The input field received a key event. This should return kKeyEventHandled if
// it handled the event, or kEventNotHandled if it should be forwarded to the
// input's super class.
- (KeyEventHandled)keyEvent:(NSEvent*)event forInput:(id)sender;

// Input field or its editor received a mouseDown: message.
- (void)onMouseDown:(NSControl<AutofillInputField>*)sender;

// An input field just became first responder.
- (void)fieldBecameFirstResponder:(NSControl<AutofillInputField>*)field;

// The user made changes to the value in the field. This is only invoked by
// AutofillTextFields.
- (void)didChange:(id)sender;

// The user is done with this field. This indicates a loss of firstResponder
// status.
- (void)didEndEditing:(id)sender;

@end

// Protocol to allow access to any given input field in an Autofill dialog, no
// matter what the underlying control is. All controls act as proxies for their
// cells, so inherits from AutofillInputCell.
@protocol AutofillInputField

@property(nonatomic, assign) id<AutofillInputDelegate> inputDelegate;

@property(nonatomic, copy) NSString* fieldValue;
@property(nonatomic, copy) NSString* defaultValue;

// Indicates if the field is at its default setting.
@property(nonatomic, readonly) BOOL isDefault;

// Indicates if the field is valid. Empty string or nil indicates a valid
// field, everything else is a message to be displayed to the user when the
// field has firstResponder status.
@property(nonatomic, copy) NSString* validityMessage;

// A reflection of the state of the validityMessage described above.
@property(nonatomic, readonly) BOOL invalid;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_INPUT_FIELD_H_

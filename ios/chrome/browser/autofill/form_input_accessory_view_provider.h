// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_PROVIDER_H_

@protocol FormInputAccessoryViewDelegate;
@protocol FormInputAccessoryViewProvider;
@class FormInputAccessoryViewController;

// Block type to indicate that a FormInputAccessoryViewProvider has an accessory
// view to provide.
typedef void (^AccessoryViewAvailableCompletion)(
    BOOL inputAccessoryViewAvailable);

// Block type to provide an accessory view asynchronously.
typedef void (^AccessoryViewReadyCompletion)(
    UIView* view,
    id<FormInputAccessoryViewProvider> provider);

// Represents an object that can provide a custom keyboard input accessory view.
@protocol FormInputAccessoryViewProvider<NSObject>

// A delegate for form navigation.
@property(nonatomic, assign) id<FormInputAccessoryViewDelegate>
    accessoryViewDelegate;

// Determines asynchronously if this provider has a view available for the
// specified form/field and invokes |completionHandler| with the answer.
- (void)
checkIfAccessoryViewIsAvailableForForm:(const web::FormActivityParams&)params
                              webState:(web::WebState*)webState
                     completionHandler:
                         (AccessoryViewAvailableCompletion)completionHandler;

// Asynchronously retrieves an accessory view from this provider for the
// specified form/field and returns it via |accessoryViewUpdateBlock|.
- (void)retrieveAccessoryViewForForm:(const web::FormActivityParams&)params
                            webState:(web::WebState*)webState
            accessoryViewUpdateBlock:
                (AccessoryViewReadyCompletion)accessoryViewUpdateBlock;

// Notifies this provider that the accessory view is going away.
- (void)inputAccessoryViewControllerDidReset:
    (FormInputAccessoryViewController*)controller;

// Notifies this provider that the accessory view frame is changing. If the
// view provided by this provider needs to change, the updated view should be
// set using |accessoryViewUpdateBlock|.
- (void)resizeAccessoryView;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_PROVIDER_H_

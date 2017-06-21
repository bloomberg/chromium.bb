// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_IOS_H_

#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"

@protocol OmniboxTextFieldDelegate<UITextFieldDelegate>

@optional
// Called when the OmniboxTextField performs a copy operation.  Returns YES if
// the delegate handled the copy operation itself.  If the delegate returns NO,
// the field must perform the copy.  Some platforms (iOS 4) do not expose an API
// that allows the delegate to handle the copy.
- (BOOL)onCopy;

// Called before the OmniboxTextField performs a paste operation.
- (void)willPaste;

// Called when the backspace button is tapped in the OmniboxTextField.
- (void)onDeleteBackward;
@end

// Enum type specifying the direction of fade animations.
typedef enum {
  OMNIBOX_TEXT_FIELD_FADE_STYLE_IN,
  OMNIBOX_TEXT_FIELD_FADE_STYLE_OUT
} OmniboxTextFieldFadeStyle;

// UITextField subclass to allow for adjusting borders.
@interface OmniboxTextFieldIOS : UITextField

// Initialize the omnibox with the given frame, font, text color, and tint
// color.
- (instancetype)initWithFrame:(CGRect)frame
                         font:(UIFont*)font
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Delegate getter and setter.  Overridden to use OmniboxTextFieldDelegate
// instead of UITextFieldDelegate.
- (id<OmniboxTextFieldDelegate>)delegate;
- (void)setDelegate:(id<OmniboxTextFieldDelegate>)delegate;

// Sets the field's text to |text|.  If |userTextLength| is less than the length
// of |text|, the excess is displayed as inline autocompleted text.  When the
// field is not in editing mode, it will respect the text attributes set on
// |text|.
- (void)setText:(NSAttributedString*)text userTextLength:(size_t)userTextLength;

// Inserts the given |text| into the text field.  The text replaces the current
// selection, if there is one; otherwise the text is inserted at the current
// cursor position.  This method should only be called while editing.
- (void)insertTextWhileEditing:(NSString*)text;

// Returns the text that is displayed in the field, including any inline
// autocomplete text that may be present.
- (base::string16)displayedText;

// Returns just the portion of |-displayedText| that is inline autocompleted.
- (base::string16)autocompleteText;

// Returns YES if this field is currently displaying any inline autocompleted
// text.
- (BOOL)hasAutocompleteText;

// Removes any inline autocompleted text that may be present.  Any text that is
// actually present in the field (not inline autocompleted) remains untouched.
- (void)clearAutocompleteText;

// On iOS 5.0+, returns any marked text in the field.  Marked text is text that
// is part of a pending IME composition.  It is an error to call this function
// on older version of iOS.
- (NSString*)markedText;

// Display a placeholder image. There is no iOS concept of placeholder images,
// circumventing it by using leftView property of UITextField and controlling
// its visibility programatically.
- (void)showPlaceholderImage;

// Hide a placeholder image. There is no iOS concept of placeholder images,
// circumventing it by using leftView property of UITextField and controlling
// its visibility programatically.
- (void)hidePlaceholderImage;

// Select which placeholder image to display.
- (void)setPlaceholderImage:(int)imageId;

// Initial touch on the Omnibox triggers a "pre-edit" state. The current
// URL is shown without any insertion point. First character typed replaces
// the URL. A second touch turns on the insertion point. |preEditStaticLabel|
// is normally hidden. In pre-edit state, |preEditStaticLabel| is unhidden
// and displays the URL that will be edited on the second touch.
- (void)enterPreEditState;
- (void)exitPreEditState;
- (BOOL)isPreEditing;

// Enable or disable the padlock button.
- (void)enableLeftViewButton:(BOOL)isEnabled;

// Returns the current selected text range as an NSRange.
- (NSRange)selectedNSRange;

// Returns the most likely text alignment, Left or Right, based on the best
// language match for |self.text|.
- (NSTextAlignment)bestTextAlignment;

// Checks if direction of the omnibox text changed, and updates the UITextField.
// alignment if necessary.
- (void)updateTextDirection;

// The color of the displayed text. Does not return the UITextField's textColor
// property.
- (UIColor*)displayedTextColor;

// Fade in/out the text and auxillary views depending on |style|.
- (void)animateFadeWithStyle:(OmniboxTextFieldFadeStyle)style;
// Reverses animations added by |-animateFadeWithStyle:|.
- (void)reverseFadeAnimations;
// Called when animations added by |-animateFadeWithStyle:| can be removed.
- (void)cleanUpFadeAnimations;

// Redeclare the delegate property to be the more specific
// OmniboxTextFieldDelegate.
@property(nonatomic, weak) id<OmniboxTextFieldDelegate> delegate;

@property(nonatomic, strong) NSString* preEditText;
@property(nonatomic) BOOL clearingPreEditText;
@property(nonatomic, strong) UIColor* selectedTextBackgroundColor;
@property(nonatomic, strong) UIColor* placeholderTextColor;
@property(nonatomic, assign) BOOL incognito;

@end

// A category for defining new methods that access private ivars.
@interface OmniboxTextFieldIOS (TestingUtilities)
- (UILabel*)preEditStaticLabel;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_IOS_H_

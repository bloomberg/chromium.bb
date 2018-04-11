// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"

#import <CoreText/CoreText.h>

#include "base/command_line.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/grit/components_scaled_resources.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/animation_util.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/reversed_animation.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "skia/ext/skia_utils_ios.h"
#include "third_party/google_toolbox_for_mac/src/iPhone/GTMFadeTruncatingLabel.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#include "ui/gfx/scoped_cg_context_save_gstate_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kFontSize = 16;
const CGFloat kEditingRectWidthInset = 12;
const CGFloat kClearButtonRightMarginIphone = 7;

const CGFloat kVoiceSearchButtonWidth = 36.0;

// The default omnibox text color (used while editing).
UIColor* TextColor() {
  return [UIColor colorWithWhite:(51 / 255.0) alpha:1.0];
}

NSString* const kOmniboxFadeAnimationKey = @"OmniboxFadeAnimation";

}  // namespace

@interface OmniboxTextFieldIOS ()

// Gets the bounds of the rect covering the URL.
- (CGRect)preEditLabelRectForBounds:(CGRect)bounds;
// Creates the UILabel if it doesn't already exist and adds it as a
// subview.
- (void)createSelectionViewIfNecessary;
// Helper method used to set the text of this field.  Updates the selection view
// to contain the correct inline autocomplete text.
- (void)setTextInternal:(NSAttributedString*)text
     autocompleteLength:(NSUInteger)autocompleteLength;
// Override deleteBackward so that backspace can clear query refinement chips.
- (void)deleteBackward;
// Returns the layers affected by animations added by |-animateFadeWithStyle:|.
- (NSArray*)fadeAnimationLayers;
// Returns the text that is displayed in the field, including any inline
// autocomplete text that may be present as an NSString. Returns the same
// value as -|displayedText| but prefer to use this to avoid unnecessary
// conversion from NSString to base::string16 if possible.
- (NSString*)nsDisplayedText;

@end

@implementation OmniboxTextFieldIOS {
  UILabel* _selection;
  UILabel* _preEditStaticLabel;
  UIFont* _font;
  UIColor* _displayedTextColor;
  UIColor* _displayedTintColor;
}

@synthesize preEditText = _preEditText;
@synthesize clearingPreEditText = _clearingPreEditText;
@synthesize selectedTextBackgroundColor = _selectedTextBackgroundColor;
@synthesize placeholderTextColor = _placeholderTextColor;
@synthesize incognito = _incognito;
@synthesize suggestionCommandsEndpoint = _suggestionCommandsEndpoint;

#pragma mark - Public methods
// Overload to allow for code-based initialization.
- (instancetype)initWithFrame:(CGRect)frame {
  return [self initWithFrame:frame
                        font:[UIFont systemFontOfSize:kFontSize]
                   textColor:TextColor()
                   tintColor:nil];
}

- (instancetype)initWithFrame:(CGRect)frame
                         font:(UIFont*)font
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor {
  self = [super initWithFrame:frame];
  if (self) {
    _font = font;
    _displayedTextColor = textColor;
    if (tintColor) {
      [self setTintColor:tintColor];
      _displayedTintColor = tintColor;
    } else {
      _displayedTintColor = self.tintColor;
    }
    [self setFont:_font];
    [self setTextColor:_displayedTextColor];
    [self setAutocorrectionType:UITextAutocorrectionTypeNo];
    [self setAutocapitalizationType:UITextAutocapitalizationTypeNone];
    [self setEnablesReturnKeyAutomatically:YES];
    [self setReturnKeyType:UIReturnKeyGo];
    [self setContentVerticalAlignment:UIControlContentVerticalAlignmentCenter];
    [self setSpellCheckingType:UITextSpellCheckingTypeNo];
    [self setTextAlignment:NSTextAlignmentNatural];
    [self setKeyboardType:(UIKeyboardType)UIKeyboardTypeWebSearch];

    if (IsRefreshLocationBarEnabled()) {
      [self setClearButtonMode:UITextFieldViewModeWhileEditing];
      [self setRightViewMode:UITextFieldViewModeNever];
    } else {
      [self setClearButtonMode:UITextFieldViewModeNever];
      [self setRightViewMode:UITextFieldViewModeAlways];
    }

#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
    if (@available(iOS 11.0, *)) {
      [self setSmartQuotesType:UITextSmartQuotesTypeNo];
    }
#endif

    // Sanity check:
    DCHECK([self conformsToProtocol:@protocol(UITextInput)]);

    // Force initial layout of internal text label.  Needed for omnibox
    // animations that will otherwise animate the text label from origin {0, 0}.
    [super setText:@" "];
  }
  return self;
}

- (instancetype)initWithCoder:(nonnull NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)setText:(NSAttributedString*)text
    userTextLength:(size_t)userTextLength {
  DCHECK_LE(userTextLength, [text length]);

  NSUInteger autocompleteLength = [text length] - userTextLength;
  [self setTextInternal:text autocompleteLength:autocompleteLength];
}

- (void)insertTextWhileEditing:(NSString*)text {
  // This method should only be called while editing.
  DCHECK([self isFirstResponder]);

  if ([self markedTextRange] != nil)
    [self unmarkText];

  NSRange selectedNSRange = [self selectedNSRange];
  if (![self delegate] || [[self delegate] textField:self
                              shouldChangeCharactersInRange:selectedNSRange
                                          replacementString:text]) {
    [self replaceRange:[self selectedTextRange] withText:text];
  }
}

- (base::string16)displayedText {
  return base::SysNSStringToUTF16([self nsDisplayedText]);
}

- (base::string16)autocompleteText {
  DCHECK_LT([[self text] length], [[_selection text] length])
      << "[_selection text] and [self text] are out of sync. "
      << "Please email justincohen@ and rohitrao@ if you see this.";
  if (_selection && [[_selection text] length] > [[self text] length]) {
    return base::SysNSStringToUTF16(
        [[_selection text] substringFromIndex:[[self text] length]]);
  }
  return base::string16();
}

- (BOOL)hasAutocompleteText {
  return !!_selection;
}

- (void)clearAutocompleteText {
  if (_selection) {
    [_selection removeFromSuperview];
    _selection = nil;
    [self showTextAndCursor];
  }
}

- (NSString*)markedText {
  DCHECK([self conformsToProtocol:@protocol(UITextInput)]);
  return [self textInRange:[self markedTextRange]];
}

- (NSRange)selectedNSRange {
  DCHECK([self isFirstResponder]);
  UITextPosition* beginning = [self beginningOfDocument];
  UITextRange* selectedRange = [self selectedTextRange];
  NSInteger start =
      [self offsetFromPosition:beginning toPosition:[selectedRange start]];
  NSInteger length = [self offsetFromPosition:[selectedRange start]
                                   toPosition:[selectedRange end]];
  return NSMakeRange(start, length);
}

- (NSTextAlignment)bestTextAlignment {
  if ([self isFirstResponder]) {
    return [self bestAlignmentForText:[self text]];
  }
  return NSTextAlignmentNatural;
}

// Normally NSTextAlignmentNatural would handle text alignment automatically,
// but there are numerous edge case issues with it, so it's simpler to just
// manually update the text alignment and writing direction of the UITextField.
- (void)updateTextDirection {
  // Setting the empty field to Natural seems to let iOS update the cursor
  // position when the keyboard language is changed.
  if (![self text].length) {
    [self setTextAlignment:NSTextAlignmentNatural];
    return;
  }

  NSTextAlignment alignment = [self bestTextAlignment];
  [self setTextAlignment:alignment];
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    // TODO(crbug.com/730461): Remove this entire block once it's been tested
    // on trunk.
    UITextWritingDirection writingDirection =
        alignment == NSTextAlignmentLeft ? UITextWritingDirectionLeftToRight
                                         : UITextWritingDirectionRightToLeft;
    [self
        setBaseWritingDirection:writingDirection
                       forRange:
                           [self
                               textRangeFromPosition:[self beginningOfDocument]
                                          toPosition:[self endOfDocument]]];
  }
}

- (UIColor*)displayedTextColor {
  return _displayedTextColor;
}

#pragma mark animations

- (void)animateFadeWithStyle:(OmniboxTextFieldFadeStyle)style {
  // Animation values
  BOOL isFadingIn = (style == OMNIBOX_TEXT_FIELD_FADE_STYLE_IN);
  CGFloat beginOpacity = isFadingIn ? 0.0 : 1.0;
  CGFloat endOpacity = isFadingIn ? 1.0 : 0.0;
  CAMediaTimingFunction* opacityTiming = ios::material::TimingFunction(
      isFadingIn ? ios::material::CurveEaseOut : ios::material::CurveEaseIn);
  CFTimeInterval delay = isFadingIn ? ios::material::kDuration8 : 0.0;

  CAAnimation* labelAnimation = OpacityAnimationMake(beginOpacity, endOpacity);
  labelAnimation.duration =
      isFadingIn ? ios::material::kDuration6 : ios::material::kDuration8;
  labelAnimation.timingFunction = opacityTiming;
  labelAnimation = DelayedAnimationMake(labelAnimation, delay);
  CAAnimation* auxillaryViewAnimation =
      OpacityAnimationMake(beginOpacity, endOpacity);
  auxillaryViewAnimation.duration = ios::material::kDuration8;
  auxillaryViewAnimation.timingFunction = opacityTiming;
  auxillaryViewAnimation = DelayedAnimationMake(auxillaryViewAnimation, delay);

  for (UIView* subview in self.subviews) {
    if ([subview isKindOfClass:[UILabel class]]) {
      [subview.layer addAnimation:labelAnimation
                           forKey:kOmniboxFadeAnimationKey];
    } else {
      [subview.layer addAnimation:auxillaryViewAnimation
                           forKey:kOmniboxFadeAnimationKey];
    }
  }
}

- (void)cleanUpFadeAnimations {
  RemoveAnimationForKeyFromLayers(kOmniboxFadeAnimationKey,
                                  [self fadeAnimationLayers]);
}

- (void)addExpandOmniboxAnimations:(UIViewPropertyAnimator*)animator
                completionAnimator:(UIViewPropertyAnimator*)completionAnimator {
  DCHECK(!IsRefreshLocationBarEnabled());

  // Hide the rightView button so its not visibile on its initial layout
  // while the expan animation is happening.
  self.clearButtonView.hidden = YES;
  self.clearButtonView.alpha = 0;
  self.clearButtonView.frame =
      CGRectLayoutOffset([self rightViewRectForBounds:self.bounds],
                         [self clearButtonAnimationOffset]);

  [completionAnimator addAnimations:^{
    self.clearButtonView.hidden = NO;
    self.clearButtonView.alpha = 1.0;

    self.clearButtonView.frame = CGRectLayoutOffset(
        self.clearButtonView.frame, -[self clearButtonAnimationOffset]);
  }];
}

- (void)addContractOmniboxAnimations:(UIViewPropertyAnimator*)animator {
  DCHECK(!IsRefreshLocationBarEnabled());

  [animator addAnimations:^{
    self.clearButtonView.alpha = 0;
    self.clearButtonView.frame = CGRectLayoutOffset(
        self.clearButtonView.frame, kToolbarButtonAnimationOffset);
  }];
  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    [self resetClearButton];
  }];
}

#pragma mark pre-edit

// Creates a UILabel based on the current dimension of the text field and
// displays the URL in the UILabel so it appears properly aligned to the URL.
- (void)enterPreEditState {
  // Empty omnibox should show the insertion point immediately. There is
  // nothing to erase.
  if (!self.text.length || UIAccessibilityIsVoiceOverRunning())
    return;

  // Remembers the initial text input to compute the diff of what was there
  // and what was typed.
  [self setPreEditText:self.text];

  // Adjusts the placement so static URL lines up perfectly with UITextField.
  DCHECK(!_preEditStaticLabel);
  CGRect rect = [self preEditLabelRectForBounds:self.bounds];
  _preEditStaticLabel = [[UILabel alloc] initWithFrame:rect];
  _preEditStaticLabel.backgroundColor = [UIColor clearColor];
  _preEditStaticLabel.opaque = YES;
  _preEditStaticLabel.font = _font;
  _preEditStaticLabel.textColor = _displayedTextColor;
  _preEditStaticLabel.lineBreakMode = NSLineBreakByTruncatingHead;

  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  // URLs have their text direction set to to LTR (avoids RTL characters
  // making the URL render from right to left, as per the URL rendering standard
  // described here: https://url.spec.whatwg.org/#url-rendering
  [style setBaseWritingDirection:NSWritingDirectionLeftToRight];
  NSDictionary* attributes = @{
    NSBackgroundColorAttributeName : [self selectedTextBackgroundColor],
    NSParagraphStyleAttributeName : style
  };

  NSAttributedString* preEditString =
      [[NSAttributedString alloc] initWithString:self.text
                                      attributes:attributes];
  [_preEditStaticLabel setAttributedText:preEditString];
  _preEditStaticLabel.textAlignment = [self preEditTextAlignment];
  [self addSubview:_preEditStaticLabel];
}

// Finishes pre-edit state by removing the UILabel with the URL.
- (void)exitPreEditState {
  [self setPreEditText:nil];
  if (_preEditStaticLabel) {
    [_preEditStaticLabel removeFromSuperview];
    _preEditStaticLabel = nil;
    [self showTextAndCursor];
  }
}

// Returns whether we are processing the first touch event on the text field.
- (BOOL)isPreEditing {
  return !![self preEditText];
}

#pragma mark - TestingUtilities category

// Exposed for testing.
- (UILabel*)preEditStaticLabel {
  return _preEditStaticLabel;
}

#pragma mark - Properties

// Enforces that the delegate is an OmniboxTextFieldDelegate.
- (id<OmniboxTextFieldDelegate>)delegate {
  id delegate = [super delegate];
  DCHECK(delegate == nil ||
         [[delegate class]
             conformsToProtocol:@protocol(OmniboxTextFieldDelegate)]);
  return delegate;
}

// Overridden to require an OmniboxTextFieldDelegate.
- (void)setDelegate:(id<OmniboxTextFieldDelegate>)delegate {
  [super setDelegate:delegate];
}

#pragma mark - Private methods

#pragma mark - UITextField

// Ensures that attributedText always uses the proper style attributes.
- (void)setAttributedText:(NSAttributedString*)attributedText {
  NSMutableAttributedString* mutableText = [attributedText mutableCopy];
  NSRange entireString = NSMakeRange(0, [mutableText length]);

  // Set the font.
  [mutableText addAttribute:NSFontAttributeName value:_font range:entireString];

  // When editing, use the default text color for all text.
  if (self.editing) {
    // Hide the text when the |_selection| label is displayed.
    UIColor* textColor =
        _selection ? [UIColor clearColor] : _displayedTextColor;
    [mutableText addAttribute:NSForegroundColorAttributeName
                        value:textColor
                        range:entireString];
  } else {
    NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
    // URLs have their text direction set to to LTR (avoids RTL characters
    // making the URL render from right to left, as per the URL rendering
    // standard described here: https://url.spec.whatwg.org/#url-rendering
    [style setBaseWritingDirection:NSWritingDirectionLeftToRight];

    // Set linebreak mode to 'clipping' to ensure the text is never elided.
    // This is a workaround for iOS 6, where it appears that
    // [self.attributedText size] is not wide enough for the string (e.g. a URL
    // else ending with '.com' will be elided to end with '.c...'). It appears
    // to be off by one point so clipping is acceptable as it doesn't actually
    // cut off any of the text.
    [style setLineBreakMode:NSLineBreakByClipping];

    [mutableText addAttribute:NSParagraphStyleAttributeName
                        value:style
                        range:entireString];
  }

  [super setAttributedText:mutableText];
}

- (void)setPlaceholder:(NSString*)placeholder {
  if (placeholder && _placeholderTextColor) {
    NSDictionary* attributes =
        @{NSForegroundColorAttributeName : _placeholderTextColor};
    self.attributedPlaceholder =
        [[NSAttributedString alloc] initWithString:placeholder
                                        attributes:attributes];
  } else {
    [super setPlaceholder:placeholder];
  }
}

- (void)setText:(NSString*)text {
  NSAttributedString* as = [[NSAttributedString alloc] initWithString:text];
  if (self.text.length > 0 && as.length == 0) {
    // Remove the fade animations before the subviews are removed.
    [self cleanUpFadeAnimations];
  }
  [self setTextInternal:as autocompleteLength:0];
}

- (CGRect)textRectForBounds:(CGRect)bounds {
  CGRect newBounds = [super textRectForBounds:bounds];

  LayoutRect textRectLayout =
      LayoutRectForRectInBoundingRect(newBounds, bounds);

  return LayoutRectGetRect(textRectLayout);
}

- (CGRect)editingRectForBounds:(CGRect)bounds {
  CGRect newBounds = [super editingRectForBounds:bounds];

  if (!IsRefreshLocationBarEnabled()) {
    // -editingRectForBounds doesn't account for rightViews that aren't flush
    // with the right edge, it just looks at the rightView's width.  Account for
    // the offset here.
    CGFloat rightViewMaxX = CGRectGetMaxX([self rightViewRectForBounds:bounds]);
    if (rightViewMaxX)
      newBounds.size.width -= bounds.size.width - rightViewMaxX;

    LayoutRect editingRectLayout =
        LayoutRectForRectInBoundingRect(newBounds, bounds);
    editingRectLayout.size.width -= kEditingRectWidthInset;
    if (IsIPadIdiom()) {
      if (!IsCompactTablet() && !self.rightView) {
        // Normally the clear button shrinks the edit box, but if the rightView
        // isn't set, shrink behind the mic icons.
        editingRectLayout.size.width -= kVoiceSearchButtonWidth;
      }
    }
    // Don't let the edit rect extend over the clear button.  The right view
    // is hidden during animations, so fake its width here.
    if (self.rightViewMode == UITextFieldViewModeNever)
      editingRectLayout.size.width -= self.rightView.bounds.size.width;

    newBounds = LayoutRectGetRect(editingRectLayout);
  }

  // Position the selection view appropriately.
  [_selection setFrame:newBounds];

  return newBounds;
}

// Overriding this method to offset the rightView property
// (containing a clear text button).
- (CGRect)rightViewRectForBounds:(CGRect)bounds {
  if (IsRefreshLocationBarEnabled()) {
    return [super rightViewRectForBounds:bounds];
  }

  // iOS9 added updated RTL support, but only half implemented it for
  // UITextField. leftView and rightView were not renamed, but are are correctly
  // swapped and treated as leadingView / trailingView.  However,
  // -leftViewRectForBounds and -rightViewRectForBounds are *not* treated as
  // leading and trailing.  Hence the swapping below.
  if ([self isTextFieldLTR]) {
    return [self layoutRightViewForBounds:bounds];
  }
  return [self layoutLeftViewForBounds:bounds];
}

// Overriding this method to offset the leftView property
// (containing a placeholder image) consistently with omnibox text padding.
- (CGRect)leftViewRectForBounds:(CGRect)bounds {
  if (IsRefreshLocationBarEnabled()) {
    return [super leftViewRectForBounds:bounds];
  }

  // iOS9 added updated RTL support, but only half implemented it for
  // UITextField. leftView and rightView were not renamed, but are are correctly
  // swapped and treated as leadingView / trailingView.  However,
  // -leftViewRectForBounds and -rightViewRectForBounds are *not* treated as
  // leading and trailing.  Hence the swapping below.
  if ([self isTextFieldLTR]) {
    return [self layoutLeftViewForBounds:bounds];
  }
  return [self layoutRightViewForBounds:bounds];
}

#pragma mark - UITextInput

- (void)beginFloatingCursorAtPoint:(CGPoint)point {
  // Exit preedit because it blocks the view of the textfield.
  [self exitPreEditState];
  [super beginFloatingCursorAtPoint:point];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  if ([self isPreEditing]) {
    CGRect rect = [self preEditLabelRectForBounds:self.bounds];
    [_preEditStaticLabel setFrame:rect];

    // Update text alignment since the pre-edit label's frame changed.
    _preEditStaticLabel.textAlignment = [self preEditTextAlignment];
    [self hideTextAndCursor];
  } else if (!_selection) {
    [self showTextAndCursor];
  }
}

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  // Anything in the narrow bar above OmniboxTextFieldIOS view
  // will also activate the text field.
  if (point.y < 0)
    point.y = 0;
  return [super hitTest:point withEvent:event];
}

#pragma mark - UIResponder

// Method called when the users touches the text input. This will accept the
// autocompleted text.
- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  if ([self isPreEditing]) {
    [self exitPreEditState];
    [super selectAll:nil];
  }

  if (!_selection) {
    [super touchesBegan:touches withEvent:event];
    return;
  }

  // Only consider a single touch.
  UITouch* touch = [touches anyObject];
  if (!touch)
    return;

  // Accept selection.
  NSString* newText = [[self nsDisplayedText] copy];
  [self clearAutocompleteText];
  [self setText:newText];
}

- (void)select:(id)sender {
  if ([self isPreEditing]) {
    [self exitPreEditState];
  }
  [super select:sender];
}

- (void)selectAll:(id)sender {
  if ([self isPreEditing]) {
    [self exitPreEditState];
  }
  if (_selection) {
    NSString* newText = [[self nsDisplayedText] copy];
    [self clearAutocompleteText];
    [self setText:newText];
  }
  [super selectAll:sender];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  // Disable the "Define" menu item.  iOS7 implements this with a private
  // selector.  Avoid using private APIs by instead doing a string comparison.
  if ([NSStringFromSelector(action) hasSuffix:@"define:"]) {
    return NO;
  }

  // Disable the RTL arrow menu item. The omnibox sets alignment based on the
  // text in the field, and should not be overridden.
  if ([NSStringFromSelector(action) hasPrefix:@"makeTextWritingDirection"]) {
    return NO;
  }

  if ([self isPreEditing]) {
    // Allow cut/copy/paste in preedit.
    if ((action == @selector(copy:)) || (action == @selector(cut:))) {
      return YES;
    }
  }

  return [super canPerformAction:action withSender:sender];
}

#pragma mark Copy/Paste

// Overridden to allow for custom omnibox copy behavior.  This includes
// preprending http:// to the copied URL if needed.
- (void)copy:(id)sender {
  id<OmniboxTextFieldDelegate> delegate = [self delegate];
  BOOL handled = NO;

  // Must test for the onCopy method, since it's optional.
  if ([delegate respondsToSelector:@selector(onCopy)])
    handled = [delegate onCopy];

  // iOS 4 doesn't expose an API that allows the delegate to handle the copy
  // operation, so let the superclass perform the copy if the delegate couldn't.
  if (!handled)
    [super copy:sender];
}

- (void)cut:(id)sender {
  if ([self isPreEditing]) {
    [self copy:sender];
    [self exitPreEditState];
    NSAttributedString* emptyString = [[NSAttributedString alloc] init];
    [self setText:emptyString userTextLength:0];
  } else {
    [super cut:sender];
  }
}

// Overridden to notify the delegate that a paste is in progress.
- (void)paste:(id)sender {
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(willPaste)])
    [delegate willPaste];
  [super paste:sender];
}

#pragma mark UIKeyInput

- (void)deleteBackward {
  // Must test for the onDeleteBackward method, since it's optional.
  if ([[self delegate] respondsToSelector:@selector(onDeleteBackward)])
    [[self delegate] onDeleteBackward];
  [super deleteBackward];
}

#pragma mark Key Commands

- (NSArray<UIKeyCommand*>*)upDownCommands {
  // These up/down arrow key commands override the standard UITextInput handling
  // of up/down arrow key. The standard behavior is to go to the beginning/end
  // of the text. Instead, the omnibox popup needs to highlight suggestions.
  UIKeyCommand* commandUp =
      [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow
                          modifierFlags:0
                                 action:@selector(keyCommandUp)];
  UIKeyCommand* commandDown =
      [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow
                          modifierFlags:0
                                 action:@selector(keyCommandDown)];

  return @[ commandUp, commandDown ];
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  NSMutableArray<UIKeyCommand*>* commands = [[self upDownCommands] mutableCopy];
  if ([self isPreEditing] || [self hasAutocompleteText]) {
    [commands addObjectsFromArray:[self leftRightCommands]];
  }

  return commands;
}

- (void)keyCommandUp {
  [self.suggestionCommandsEndpoint highlightNextSuggestion];
}

- (void)keyCommandDown {
  [self.suggestionCommandsEndpoint highlightPreviousSuggestion];
}

#pragma mark preedit and inline autocomplete key commands

// React to left and right keys when in preedit state to exit preedit and put
// cursor to the beginning/end of the textfield; or if there is inline
// suggestion displayed, accept it and put the cursor before/after the
// suggested text.
- (NSArray<UIKeyCommand*>*)leftRightCommands {
  UIKeyCommand* commandLeft =
      [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow
                          modifierFlags:0
                                 action:@selector(keyCommandLeft)];
  UIKeyCommand* commandRight =
      [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow
                          modifierFlags:0
                                 action:@selector(keyCommandRight)];

  return @[ commandLeft, commandRight ];
}

- (void)keyCommandLeft {
  DCHECK([self isPreEditing] || [self hasAutocompleteText]);

  // Cursor offset.
  NSInteger offset = 0;
  if ([self isPreEditing]) {
    [self exitPreEditState];
  }

  if ([self hasAutocompleteText]) {
    // The cursor should stay in the end of the user input.
    offset = self.text.length;

    // Accept autocomplete suggestion.
    [self acceptAutocompleteText];
  }

  UITextPosition* beginning = self.beginningOfDocument;
  UITextPosition* cursorPosition =
      [self positionFromPosition:beginning offset:offset];
  UITextRange* textRange =
      [self textRangeFromPosition:cursorPosition toPosition:cursorPosition];
  self.selectedTextRange = textRange;
}

- (void)keyCommandRight {
  DCHECK([self isPreEditing] || [self hasAutocompleteText]);

  if ([self isPreEditing]) {
    [self exitPreEditState];
  }

  if ([self hasAutocompleteText]) {
    [self acceptAutocompleteText];
  }

  // Put the cursor to the end of the input.
  UITextPosition* end = self.endOfDocument;
  UITextRange* textRange = [self textRangeFromPosition:end toPosition:end];

  self.selectedTextRange = textRange;
}

// A helper to accept the current autocomplete text.
- (void)acceptAutocompleteText {
  DCHECK([self hasAutocompleteText]);
  // Strip attributes and set text as if the user typed it.
  NSAttributedString* string =
      [[NSAttributedString alloc] initWithString:_selection.text];
  [self setText:string userTextLength:string.length];
}

#pragma mark - helpers

// Gets the bounds of the rect covering the URL.
- (CGRect)preEditLabelRectForBounds:(CGRect)bounds {
  return [self editingRectForBounds:self.bounds];
}

- (NSTextAlignment)bestAlignmentForText:(NSString*)text {
  if (text.length) {
    NSString* lang = CFBridgingRelease(CFStringTokenizerCopyBestStringLanguage(
        (CFStringRef)text, CFRangeMake(0, text.length)));

    if ([NSLocale characterDirectionForLanguage:lang] ==
        NSLocaleLanguageDirectionRightToLeft) {
      return NSTextAlignmentRight;
    }
  }
  return NSTextAlignmentLeft;
}

- (NSTextAlignment)preEditTextAlignment {
  // If the pre-edit text is wider than the omnibox, right-align the text so it
  // ends at the same x coord as the blue selection box.
  CGSize textSize =
      [_preEditStaticLabel.text cr_pixelAlignedSizeWithFont:_font];
  // Note, this does not need to support RTL, as URLs are always LTR.
  return textSize.width < _preEditStaticLabel.frame.size.width
             ? NSTextAlignmentLeft
             : NSTextAlignmentRight;
}

- (NSString*)nsDisplayedText {
  if (_selection)
    return [_selection text];
  return [self text];
}

// Creates the SelectedTextLabel if it doesn't already exist and adds it as a
// subview.
- (void)createSelectionViewIfNecessary {
  if (_selection)
    return;

  _selection = [[UILabel alloc] initWithFrame:CGRectZero];
  [_selection setFont:_font];
  [_selection setTextColor:_displayedTextColor];
  [_selection setOpaque:NO];
  [_selection setBackgroundColor:[UIColor clearColor]];
  [self addSubview:_selection];
  [self hideTextAndCursor];
}

// Helper method used to set the text of this field.  Updates the selection view
// to contain the correct inline autocomplete text.
- (void)setTextInternal:(NSAttributedString*)text
     autocompleteLength:(NSUInteger)autocompleteLength {
  // Extract substrings for the permanent text and the autocomplete text.  The
  // former needs to retain any text attributes from the original string.
  NSRange fieldRange = NSMakeRange(0, [text length] - autocompleteLength);
  NSAttributedString* fieldText =
      [text attributedSubstringFromRange:fieldRange];

  if (autocompleteLength > 0) {
    // Creating |autocompleteText| from |[text string]| has the added bonus of
    // removing all the previously set attributes. This way the autocomplete
    // text doesn't have a highlighted protocol, etc.
    NSMutableAttributedString* autocompleteText =
        [[NSMutableAttributedString alloc] initWithString:[text string]];

    [self createSelectionViewIfNecessary];
    DCHECK(_selection);
    [autocompleteText
        addAttribute:NSBackgroundColorAttributeName
               value:[self selectedTextBackgroundColor]
               range:NSMakeRange([fieldText length], autocompleteLength)];
    [_selection setAttributedText:autocompleteText];
    [_selection setTextAlignment:[self bestTextAlignment]];
  } else {
    [self clearAutocompleteText];
  }

  // The following BOOL was introduced to workaround a UIKit bug
  // (crbug.com/737589, rdar/32817402). The bug relates to third party keyboards
  // that check the value of textDocumentProxy.documentContextBeforeInput to
  // show keyboard suggestions. It appears that calling setAttributedText during
  // an EditingChanged UIControlEvent somehow triggers this bug. The reason we
  // update the attributed text here is to change the colors of the omnibox
  // (such as host, protocol) when !self.editing, but also to hide real
  // UITextField text under the _selection text when self.editing. Since we will
  // correct the omnibox editing text color anytime |self.text| is different
  // than |fieldText|, it seems it's OK to skip calling self.attributedText
  // during the condition added below. If we change mobile omnibox to match
  // desktop and also color the omnibox while self.editing, this workaround will
  // no longer work. The check for |autocompleteLength| reduces the scope of
  // this workaround, without it having introduced crbug.com/740075.
  BOOL updateText = YES;
  if (experimental_flags::IsThirdPartyKeyboardWorkaroundEnabled()) {
    updateText =
        (!self.editing || ![self.text isEqualToString:fieldText.string] ||
         autocompleteLength == 0);
  }
  if (updateText) {
    self.attributedText = fieldText;
  }

  // iOS changes the font to .LastResort when some unexpected unicode strings
  // are used (e.g. 𝗲𝗺𝗽𝗵𝗮𝘀𝗶𝘀).  Setting the NSFontAttributeName in the
  // attributed string to -systemFontOfSize fixes part of the problem, but the
  // baseline changes so text is out of alignment.
  [self setFont:_font];
  [self updateTextDirection];
}

- (UIColor*)selectedTextBackgroundColor {
  return _selectedTextBackgroundColor ? _selectedTextBackgroundColor
                                      : [UIColor colorWithRed:204.0 / 255
                                                        green:221.0 / 255
                                                         blue:237.0 / 255
                                                        alpha:1.0];
}

- (BOOL)isColorHidden:(UIColor*)color {
  return ([color isEqual:[UIColor clearColor]] ||
          CGColorGetAlpha(color.CGColor) < 0.05);
}

// Set the text field's text and cursor to their displayed colors. To be called
// when there are no overlaid views displayed.
- (void)showTextAndCursor {
  if ([self isColorHidden:self.textColor]) {
    [self setTextColor:_displayedTextColor];
  }
  if ([self isColorHidden:self.tintColor]) {
    [self setTintColor:_displayedTintColor];
  }
}

// Set the text field's text and cursor to clear so that they don't show up
// behind any overlaid views.
- (void)hideTextAndCursor {
  [self setTintColor:[UIColor clearColor]];
  [self setTextColor:[UIColor clearColor]];
}

- (NSArray*)fadeAnimationLayers {
  NSMutableArray* layers = [NSMutableArray array];
  for (UIView* subview in self.subviews)
    [layers addObject:subview.layer];
  return layers;
}

- (BOOL)isTextFieldLTR {
  return [[self class] userInterfaceLayoutDirectionForSemanticContentAttribute:
                           self.semanticContentAttribute] ==
         UIUserInterfaceLayoutDirectionLeftToRight;
}

- (CGRect)layoutRightViewForBounds:(CGRect)bounds {
  DCHECK(!IsRefreshLocationBarEnabled());

  if ([self rightView]) {
    CGSize rightViewSize = self.rightView.bounds.size;
    CGFloat leadingOffset = 0;
    leadingOffset =
        bounds.size.width - rightViewSize.width - kClearButtonRightMarginIphone;
    LayoutRect rightViewLayout;
    rightViewLayout.position.leading = leadingOffset;
    rightViewLayout.boundingWidth = CGRectGetWidth(bounds);
    rightViewLayout.position.originY =
        floor((bounds.size.height - rightViewSize.height) / 2.0);
    rightViewLayout.size = rightViewSize;
    return LayoutRectGetRect(rightViewLayout);
  }
  return CGRectZero;
}

- (CGRect)layoutLeftViewForBounds:(CGRect)bounds {
  return CGRectZero;
}

// Accesses the clear button view when it's available; correctly resolves RTL.
// This method must not be named -clearButton, because that conflicts with an
// internal UITextField method.
- (UIView*)clearButtonView {
  DCHECK(!IsRefreshLocationBarEnabled());
  if ([self isTextFieldLTR]) {
    return self.rightView;
  } else {
    return self.leftView;
  }
}

- (void)resetClearButton {
  DCHECK(!IsRefreshLocationBarEnabled());

  if ([self isTextFieldLTR]) {
    self.rightView = nil;
  } else {
    self.rightView = nil;
  }
}

- (CGFloat)clearButtonAnimationOffset {
  DCHECK(!IsRefreshLocationBarEnabled());

  if ([self isTextFieldLTR]) {
    return kToolbarButtonAnimationOffset;
  } else {
    return -kToolbarButtonAnimationOffset;
  }
}

@end

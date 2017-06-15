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
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/ui/animation_util.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/reversed_animation.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
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
const CGFloat kEditingRectX = 16;
const CGFloat kEditingRectWidthInset = 10;
const CGFloat kTextInset = 8;
const CGFloat kTextInsetNoLeftView = 12;
const CGFloat kImageInset = 9;
const CGFloat kClearButtonRightMarginIphone = 7;
const CGFloat kClearButtonRightMarginIpad = 12;
// Amount to shift the origin.x of the text areas so they're centered within the
// omnibox border.
const CGFloat kTextAreaLeadingOffset = -2;

// TODO(rohitrao): Should this be pulled from somewhere else?
const CGFloat kStarButtonWidth = 36;
const CGFloat kVoiceSearchButtonWidth = 36.0;

// The default omnibox text color (used while editing).
UIColor* TextColor() {
  return [UIColor colorWithWhite:(51 / 255.0) alpha:1.0];
}

NSString* const kOmniboxFadeAnimationKey = @"OmniboxFadeAnimation";

}  // namespace

@interface OmniboxTextFieldIOS ()

// Current image id used in left view.
@property(nonatomic, assign) NSUInteger leftViewImageId;

// Gets the bounds of the rect covering the URL.
- (CGRect)preEditLabelRectForBounds:(CGRect)bounds;
// Creates the UILabel if it doesn't already exist and adds it as a
// subview.
- (void)createSelectionViewIfNecessary;
// Helper method used to set the text of this field.  Updates the selection view
// to contain the correct inline autocomplete text.
- (void)setTextInternal:(NSAttributedString*)text
     autocompleteLength:(NSUInteger)autocompleteLength;
// Display an image or chip text in the left accessory view.
- (void)updateLeftView;
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

#pragma mark -
#pragma mark OmniboxTextFieldIOS

@implementation OmniboxTextFieldIOS {
  UILabel* _selection;
  UILabel* _preEditStaticLabel;
  UIFont* _font;
  UIColor* _displayedTextColor;
  UIColor* _displayedTintColor;

  // The 'Copy URL' menu item is sometimes shown in the edit menu, so keep it
  // around to make adding/removing easier.
  UIMenuItem* _copyUrlMenuItem;
}

@synthesize leftViewImageId = _leftViewImageId;
@synthesize preEditText = _preEditText;
@synthesize clearingPreEditText = _clearingPreEditText;
@synthesize selectedTextBackgroundColor = _selectedTextBackgroundColor;
@synthesize placeholderTextColor = _placeholderTextColor;
@synthesize incognito = _incognito;

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
    [self setClearButtonMode:UITextFieldViewModeNever];
    [self setRightViewMode:UITextFieldViewModeAlways];
    [self setAutocorrectionType:UITextAutocorrectionTypeNo];
    [self setAutocapitalizationType:UITextAutocapitalizationTypeNone];
    [self setEnablesReturnKeyAutomatically:YES];
    [self setReturnKeyType:UIReturnKeyGo];
    [self setContentVerticalAlignment:UIControlContentVerticalAlignmentCenter];
    [self setSpellCheckingType:UITextSpellCheckingTypeNo];
    [self setTextAlignment:NSTextAlignmentNatural];
    [self setKeyboardType:(UIKeyboardType)UIKeyboardTypeWebSearch];

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

// Exposed for testing.
- (UILabel*)preEditStaticLabel {
  return _preEditStaticLabel;
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

// Gets the bounds of the rect covering the URL.
- (CGRect)preEditLabelRectForBounds:(CGRect)bounds {
  return [self editingRectForBounds:self.bounds];
}

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

  NSDictionary* attributes =
      @{NSBackgroundColorAttributeName : [self selectedTextBackgroundColor]};
  NSAttributedString* preEditString =
      [[NSAttributedString alloc] initWithString:self.text
                                      attributes:attributes];
  [_preEditStaticLabel setAttributedText:preEditString];
  _preEditStaticLabel.textAlignment = [self preEditTextAlignment];
  [self addSubview:_preEditStaticLabel];
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

- (NSTextAlignment)bestTextAlignment {
  if ([self isFirstResponder]) {
    return [self bestAlignmentForText:[self text]];
  }
  return NSTextAlignmentNatural;
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

// Finishes pre-edit state by removing the UILabel with the URL.
- (void)exitPreEditState {
  [self setPreEditText:nil];
  if (_preEditStaticLabel) {
    [_preEditStaticLabel removeFromSuperview];
    _preEditStaticLabel = nil;
    [self showTextAndCursor];
  }
}

- (UIColor*)displayedTextColor {
  return _displayedTextColor;
}

// Returns whether we are processing the first touch event on the text field.
- (BOOL)isPreEditing {
  return !![self preEditText];
}

- (void)enableLeftViewButton:(BOOL)isEnabled {
  if ([self leftView])
    [(UIButton*)[self leftView] setEnabled:isEnabled];
}

- (NSString*)nsDisplayedText {
  if (_selection)
    return [_selection text];
  return [self text];
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

- (void)updateLeftView {
  UIButton* leftViewButton = (UIButton*)self.leftView;

  // For iPhone, the left view is only updated when not in editing mode (i.e.
  // the text field is not first responder).
  if (_leftViewImageId && (IsIPadIdiom() || ![self isFirstResponder])) {
    UIImage* image = [NativeImage(_leftViewImageId)
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
    [leftViewButton setImage:imageView.image forState:UIControlStateNormal];
    [leftViewButton setTitle:nil forState:UIControlStateNormal];
    UIColor* tint = [UIColor whiteColor];
    if (!_incognito) {
      switch (_leftViewImageId) {
        case IDR_IOS_LOCATION_BAR_HTTP:
          tint = [UIColor darkGrayColor];
          break;
        case IDR_IOS_OMNIBOX_HTTPS_VALID:
          tint = skia::UIColorFromSkColor(gfx::kGoogleGreen700);
          break;
        case IDR_IOS_OMNIBOX_HTTPS_POLICY_WARNING:
          tint = skia::UIColorFromSkColor(gfx::kGoogleYellow700);
          break;
        case IDR_IOS_OMNIBOX_HTTPS_INVALID:
          tint = skia::UIColorFromSkColor(gfx::kGoogleRed700);
          break;
        default:
          tint = [UIColor darkGrayColor];
      }
    }
    [leftViewButton setTintColor:tint];
  } else {
    // Reset the chip text.
    [leftViewButton setTitle:nil forState:UIControlStateNormal];
  }
  // Normally this isn't needed, but there is a bug in iOS 7.1+ where setting
  // the image while disabled doesn't always honor UIControlStateNormal.
  // crbug.com/355077
  [leftViewButton setNeedsLayout];

  [leftViewButton sizeToFit];
  self.leftView.isAccessibilityElement =
      self.attributedText.length != 0 && leftViewButton.isEnabled;
}

- (void)deleteBackward {
  // Must test for the onDeleteBackward method, since it's optional.
  if ([[self delegate] respondsToSelector:@selector(onDeleteBackward)])
    [[self delegate] onDeleteBackward];
  [super deleteBackward];
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

  self.attributedText = fieldText;

  // iOS changes the font to .LastResort when some unexpected unicode strings
  // are used (e.g. 𝗲𝗺𝗽𝗵𝗮𝘀𝗶𝘀).  Setting the NSFontAttributeName in the
  // attributed string to -systemFontOfSize fixes part of the problem, but the
  // baseline changes so text is out of alignment.
  [self setFont:_font];
  // TODO(justincohen): Find a better place to put this, and consolidate it with
  // the same call in omniboxViewIOS.
  [self updateTextDirection];
}

- (UIColor*)selectedTextBackgroundColor {
  return _selectedTextBackgroundColor ? _selectedTextBackgroundColor
                                      : [UIColor colorWithRed:204.0 / 255
                                                        green:221.0 / 255
                                                         blue:237.0 / 255
                                                        alpha:1.0];
}

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
    // making the URL render from right to left, as per RFC 3987 Section 4.1).
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

- (void)setText:(NSAttributedString*)text
    userTextLength:(size_t)userTextLength {
  DCHECK_LE(userTextLength, [text length]);

  NSUInteger autocompleteLength = [text length] - userTextLength;
  [self setTextInternal:text autocompleteLength:autocompleteLength];
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

- (NSString*)markedText {
  DCHECK([self conformsToProtocol:@protocol(UITextInput)]);
  return [self textInRange:[self markedTextRange]];
}

- (CGRect)textRectForBounds:(CGRect)bounds {
  CGRect newBounds = [super textRectForBounds:bounds];

  LayoutRect textRectLayout =
      LayoutRectForRectInBoundingRect(newBounds, bounds);
  CGFloat textInset = [self leftViewMode] == UITextFieldViewModeAlways
                          ? kTextInset
                          : kTextInsetNoLeftView;
  // Shift the text right and reduce the width to create empty space between the
  // left view and the omnibox text.
  textRectLayout.position.leading += textInset + kTextAreaLeadingOffset;
  textRectLayout.size.width -= textInset - kTextAreaLeadingOffset;

  if (IsIPadIdiom()) {
    if (!IsCompactTablet()) {
      // Adjust the width so that the text doesn't overlap with the bookmark and
      // voice search buttons which are displayed inside the omnibox.
      textRectLayout.size.width += self.rightView.bounds.size.width -
                                   kVoiceSearchButtonWidth - kStarButtonWidth;
    }
  } else if (self.leftView.alpha == 0) {
    CGFloat xDiff = textRectLayout.position.leading - kEditingRectX;
    textRectLayout.position.leading = kEditingRectX;
    textRectLayout.size.width += xDiff;
  }

  return LayoutRectGetRect(textRectLayout);
}

- (CGRect)editingRectForBounds:(CGRect)bounds {
  CGRect newBounds = [super editingRectForBounds:bounds];

  // -editingRectForBounds doesn't account for rightViews that aren't flush
  // with the right edge, it just looks at the rightView's width.  Account for
  // the offset here.
  CGFloat rightViewMaxX = CGRectGetMaxX([self rightViewRectForBounds:bounds]);
  if (rightViewMaxX)
    newBounds.size.width -= bounds.size.width - rightViewMaxX;

  LayoutRect editingRectLayout =
      LayoutRectForRectInBoundingRect(newBounds, bounds);
  editingRectLayout.position.leading += kTextAreaLeadingOffset;
  editingRectLayout.position.leading += kTextInset;
  editingRectLayout.size.width -= kTextInset + kEditingRectWidthInset;
  if (IsIPadIdiom()) {
    if (!IsCompactTablet() && !self.rightView) {
      // Normally the clear button shrinks the edit box, but if the rightView
      // isn't set, shrink behind the mic icons.
      editingRectLayout.size.width -= kVoiceSearchButtonWidth;
    }
  } else {
    CGFloat xDiff = editingRectLayout.position.leading - kEditingRectX;
    editingRectLayout.position.leading = kEditingRectX;
    editingRectLayout.size.width += xDiff;
  }
  // Don't let the edit rect extend over the clear button.  The right view
  // is hidden during animations, so fake its width here.
  if (self.rightViewMode == UITextFieldViewModeNever)
    editingRectLayout.size.width -= self.rightView.bounds.size.width;

  newBounds = LayoutRectGetRect(editingRectLayout);

  // Position the selection view appropriately.
  [_selection setFrame:newBounds];

  return newBounds;
}

- (CGRect)rectForDrawTextInRect:(CGRect)rect {
  // The goal is to always show the most significant part of the hostname
  // (i.e. the end of the TLD).
  //
  //                     --------------------
  // www.somereallyreally|longdomainname.com|/path/gets/clipped
  //                     --------------------
  // {  clipped prefix  } {  visible text  } { clipped suffix }

  // First find how much (if any) of the scheme/host needs to be clipped so that
  // the end of the TLD fits in |rect|. Note that if the omnibox is currently
  // displaying a search query the prefix is not clipped.
  CGFloat widthOfClippedPrefix = 0;
  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      base::SysNSStringToUTF16(self.text), AutocompleteSchemeClassifierImpl(),
      &scheme, &host);
  if (host.len < 0) {
    return rect;
  }
  NSRange hostRange = NSMakeRange(0, host.begin + host.len);
  NSAttributedString* hostString =
      [self.attributedText attributedSubstringFromRange:hostRange];
  CGFloat widthOfHost = ceil([hostString size].width);
  widthOfClippedPrefix = MAX(widthOfHost - rect.size.width, 0);

  // Now determine if there is any text that will need to be truncated because
  // there's not enough room.
  int textWidth = ceil([self.attributedText size].width);
  CGFloat widthOfClippedSuffix =
      MAX(textWidth - rect.size.width - widthOfClippedPrefix, 0);
  BOOL suffixClipped = widthOfClippedSuffix > 0;

  // Fade the beginning and/or end of the visible string to indicate to the user
  // that the URL has been clipped.
  BOOL prefixClipped = widthOfClippedPrefix > 0;
  if (prefixClipped || suffixClipped) {
    UIImage* fade = nil;
    if ([self textAlignment] == NSTextAlignmentRight) {
      // Swap prefix and suffix for RTL.
      fade = [GTMFadeTruncatingLabel getLinearGradient:rect
                                              fadeHead:suffixClipped
                                              fadeTail:prefixClipped];
    } else {
      fade = [GTMFadeTruncatingLabel getLinearGradient:rect
                                              fadeHead:prefixClipped
                                              fadeTail:suffixClipped];
    }
    CGContextClipToMask(UIGraphicsGetCurrentContext(), rect, fade.CGImage);
  }

  // If necessary, expand the rect so the entire string fits and shift it to the
  // left (right for RTL) so the clipped prefix is not shown.
  if ([self textAlignment] == NSTextAlignmentRight) {
    rect.origin.x -= widthOfClippedSuffix;
  } else {
    rect.origin.x -= widthOfClippedPrefix;
  }
  rect.size.width = MAX(rect.size.width, textWidth);
  return rect;
}

// Enumerate url components (host, path) and draw each one in different rect.
- (void)drawTextInRect:(CGRect)rect {
  // Save and restore the graphics state because rectForDrawTextInRect may
  // apply an image mask to fade out beginning and/or end of the URL.
  gfx::ScopedCGContextSaveGState saver(UIGraphicsGetCurrentContext());
  [super drawTextInRect:[self rectForDrawTextInRect:rect]];
}

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  // Anything in the narrow bar above OmniboxTextFieldIOS view
  // will also activate the text field.
  if (point.y < 0)
    point.y = 0;
  return [super hitTest:point withEvent:event];
}

- (BOOL)isTextFieldLTR {
  return [[self class] userInterfaceLayoutDirectionForSemanticContentAttribute:
                           self.semanticContentAttribute] ==
         UIUserInterfaceLayoutDirectionLeftToRight;
}

// Overriding this method to offset the rightView property
// (containing a clear text button).
- (CGRect)rightViewRectForBounds:(CGRect)bounds {
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

- (CGRect)layoutRightViewForBounds:(CGRect)bounds {
  if ([self rightView]) {
    CGSize rightViewSize = self.rightView.bounds.size;
    CGFloat leadingOffset = 0;
    if (IsIPadIdiom() && !IsCompactTablet()) {
      leadingOffset = bounds.size.width - kVoiceSearchButtonWidth -
                      rightViewSize.width - kClearButtonRightMarginIpad;
    } else {
      leadingOffset = bounds.size.width - rightViewSize.width -
                      kClearButtonRightMarginIphone;
    }
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

// Overriding this method to offset the leftView property
// (containing a placeholder image) consistently with omnibox text padding.
- (CGRect)leftViewRectForBounds:(CGRect)bounds {
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

- (CGRect)layoutLeftViewForBounds:(CGRect)bounds {
  if ([self leftView]) {
    CGSize imageSize = [[self leftView] bounds].size;
    LayoutRect leftViewLayout =
        LayoutRectMake(kImageInset, CGRectGetWidth(bounds),
                       floor((bounds.size.height - imageSize.height) / 2.0),
                       imageSize.width, imageSize.height);
    return LayoutRectGetRect(leftViewLayout);
  }
  return CGRectZero;
}

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

- (NSArray*)fadeAnimationLayers {
  NSMutableArray* layers = [NSMutableArray array];
  for (UIView* subview in self.subviews)
    [layers addObject:subview.layer];
  return layers;
}

- (void)reverseFadeAnimations {
  ReverseAnimationsForKeyForLayers(kOmniboxFadeAnimationKey,
                                   [self fadeAnimationLayers]);
}

- (void)cleanUpFadeAnimations {
  RemoveAnimationForKeyFromLayers(kOmniboxFadeAnimationKey,
                                  [self fadeAnimationLayers]);
}

#pragma mark - Placeholder image handling methods.

- (void)setPlaceholderImage:(int)imageId {
  _leftViewImageId = imageId;
  [self updateLeftView];
}

- (void)showPlaceholderImage {
  [self setLeftViewMode:UITextFieldViewModeAlways];
}

- (void)hidePlaceholderImage {
  [self setLeftViewMode:UITextFieldViewModeNever];
}

#pragma mark - Copy/Paste

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

// Overridden to notify the delegate that a paste is in progress.
- (void)paste:(id)sender {
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(willPaste)])
    [delegate willPaste];
  [super paste:sender];
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

- (BOOL)becomeFirstResponder {
  if (![super becomeFirstResponder])
    return NO;

  if (!_copyUrlMenuItem) {
    NSString* const kTitle = l10n_util::GetNSString(IDS_IOS_COPY_URL);
    _copyUrlMenuItem =
        [[UIMenuItem alloc] initWithTitle:kTitle action:@selector(copyUrl:)];
  }

  // Add the "Copy URL" menu item to the |sharedMenuController| if necessary.
  UIMenuController* menuController = [UIMenuController sharedMenuController];
  if (menuController.menuItems) {
    if (![menuController.menuItems containsObject:_copyUrlMenuItem]) {
      menuController.menuItems =
          [menuController.menuItems arrayByAddingObject:_copyUrlMenuItem];
    }
  } else {
    menuController.menuItems = [NSArray arrayWithObject:_copyUrlMenuItem];
  }
  return YES;
}

- (BOOL)resignFirstResponder {
  if (![super resignFirstResponder])
    return NO;

  // Remove the "Copy URL" menu item from the |sharedMenuController|.
  UIMenuController* menuController = [UIMenuController sharedMenuController];
  NSMutableArray* menuItems =
      [NSMutableArray arrayWithArray:menuController.menuItems];
  [menuItems removeObject:_copyUrlMenuItem];
  menuController.menuItems = menuItems;
  return YES;
}

- (void)copyUrl:(id)sender {
  [[self delegate] onCopyURL];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(copyUrl:)) {
    return [[self delegate] canCopyURL];
  }

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

  return [super canPerformAction:action withSender:sender];
}

@end

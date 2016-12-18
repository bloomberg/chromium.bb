// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/today_extension/interactive_label.h"

#import "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/common/string_util.h"
#include "ios/chrome/today_extension/transparent_button.h"
#include "ios/chrome/today_extension/ui_util.h"

@interface InteractiveLabel ()<UITextViewDelegate>

- (CGRect)frameOfTextRange:(NSRange)range;

@end

// The UITextView must have selection enabled for the link detection to work.
// The FooterLabels must not have selection enabled. Disable it by disabling
// becomeFirstResponder.
@interface NoSelectionUITextView : UITextView
@end

@implementation NoSelectionUITextView

- (BOOL)canBecomeFirstResponder {
  return NO;
}

@end

@implementation InteractiveLabel {
  base::scoped_nsobject<NSString> _labelString;
  base::scoped_nsobject<NSString> _buttonString;
  base::scoped_nsobject<UITextView> _label;
  base::mac::ScopedBlock<ProceduralBlock> _linkBlock;
  base::mac::ScopedBlock<ProceduralBlock> _buttonBlock;
  base::scoped_nsobject<TransparentButton> _activationButton;
  NSRange _buttonRange;
  base::scoped_nsobject<NSMutableAttributedString> _attributedText;
  UIEdgeInsets _insets;
  CGFloat _currentWidth;

  // These constraints set the position of the button inside
  base::scoped_nsobject<NSLayoutConstraint> _buttonTopConstraint;
  base::scoped_nsobject<NSLayoutConstraint> _buttonLeftConstraint;
  base::scoped_nsobject<NSLayoutConstraint> _buttonHeightConstraint;
  base::scoped_nsobject<NSLayoutConstraint> _buttonWidthConstraint;
}

- (instancetype)initWithFrame:(CGRect)frame
                  labelString:(NSString*)labelString
                     fontSize:(CGFloat)fontSize
               labelAlignment:(NSTextAlignment)labelAlignment
                       insets:(UIEdgeInsets)insets
                 buttonString:(NSString*)buttonString
                    linkBlock:(ProceduralBlock)linkBlock
                  buttonBlock:(ProceduralBlock)buttonBlock {
  self = [super initWithFrame:frame];
  if (self) {
    _insets = insets;
    _currentWidth = frame.size.width;
    // When the first character of the UITextView text as a NSLinkAttributeName
    // attribute, the lineSpacing attribute of the paragraph style is ignored.
    // Add a zero width space so the first character is never in a link.
    NSString* prefixedString =
        [NSString stringWithFormat:@"\u200B%@", labelString];
    _labelString.reset([prefixedString copy]);
    _linkBlock.reset(linkBlock, base::scoped_policy::RETAIN);

    _label.reset([[NoSelectionUITextView alloc] initWithFrame:CGRectZero]);
    [_label setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_label setDelegate:self];
    [_label setBackgroundColor:[UIColor clearColor]];
    [_label setSelectable:YES];
    [_label setEditable:NO];
    // We want to get rid of the padding of the text in the UITextView.
    [_label setTextContainerInset:UIEdgeInsetsZero];
    [[_label textContainer] setLineFragmentPadding:0];
    [self addSubview:_label];
    [NSLayoutConstraint activateConstraints:@[
      [[_label topAnchor] constraintEqualToAnchor:[self topAnchor]
                                         constant:_insets.top],
      [[_label bottomAnchor] constraintEqualToAnchor:[self bottomAnchor]
                                            constant:_insets.bottom],
      [[_label leadingAnchor] constraintEqualToAnchor:[self leadingAnchor]
                                             constant:_insets.left],
      [[_label trailingAnchor] constraintEqualToAnchor:[self trailingAnchor]
                                              constant:-_insets.right]
    ]];

    NSRange linkRange;
    NSString* text = ParseStringWithLink(_labelString, &linkRange);
    [_label setAccessibilityTraits:[_label accessibilityTraits] |
                                   UIAccessibilityTraitStaticText];
    NSString* buttonText = nil;
    _buttonRange = NSMakeRange(0, 0);
    BOOL showButton = buttonString && buttonBlock;
    if (showButton) {
      _buttonString.reset([buttonString copy]);
      _buttonBlock.reset(buttonBlock, base::scoped_policy::RETAIN);
      _activationButton.reset(
          [[TransparentButton alloc] initWithFrame:CGRectZero]);
      [_activationButton addTarget:self
                            action:@selector(buttonPressed:)
                  forControlEvents:UIControlEventTouchUpInside];
      [_activationButton setBackgroundColor:[UIColor clearColor]];
      [_activationButton setInkColor:ui_util::InkColor()];
      [_activationButton setCornerRadius:2];
      [_activationButton setBorderWidth:1];
      [_activationButton setBorderColor:ui_util::BorderColor()];
      [self addSubview:_activationButton];
      [self bringSubviewToFront:_activationButton];
      _buttonTopConstraint.reset([[[_activationButton topAnchor]
          constraintEqualToAnchor:[self topAnchor]] retain]);
      _buttonLeftConstraint.reset([[[_activationButton leftAnchor]
          constraintEqualToAnchor:[self leftAnchor]] retain]);
      _buttonWidthConstraint.reset([[[_activationButton widthAnchor]
          constraintEqualToConstant:0] retain]);
      _buttonHeightConstraint.reset([[[_activationButton heightAnchor]
          constraintEqualToConstant:0] retain]);
      [NSLayoutConstraint activateConstraints:@[
        _buttonTopConstraint, _buttonLeftConstraint, _buttonWidthConstraint,
        _buttonHeightConstraint
      ]];

      // Add two spaces before and after the button label to add padding to the
      // button.
      buttonText = [NSString stringWithFormat:@"  %@  ", _buttonString.get()];
      // Replace spaces by non breaking spaces to prevent buttons from wrapping.
      buttonText = [buttonText stringByReplacingOccurrencesOfString:@" "
                                                         withString:@"\u00A0"];
      // Add space between the text and the button as separator.
      text = [text stringByAppendingString:@"   "];
      _buttonRange = NSMakeRange([text length], [buttonText length]);
      text = [text stringByAppendingString:buttonText];
    }

    base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
        [[NSMutableParagraphStyle alloc] init]);
    UIFont* font = [UIFont fontWithName:@"Helvetica" size:fontSize];
    [paragraphStyle setLineBreakMode:NSLineBreakByWordWrapping];
    [paragraphStyle setLineSpacing:2];

    NSDictionary* normalAttributes = @{
      NSParagraphStyleAttributeName : paragraphStyle,
      NSFontAttributeName : font,
      NSForegroundColorAttributeName : ui_util::FooterTextColor(),
      NSBackgroundColorAttributeName : [UIColor clearColor],
    };
    NSDictionary* linkAttributes = @{
      NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle),
      NSForegroundColorAttributeName : ui_util::FooterTextColor(),
    };
    NSDictionary* hiddenAttributes = @{
      NSForegroundColorAttributeName : [UIColor clearColor],
    };

    _attributedText.reset(
        [[NSMutableAttributedString alloc] initWithString:text]);

    [_attributedText setAttributes:normalAttributes
                             range:NSMakeRange(0, text.length)];

    NSDictionary* linkURLAttributes = @{
      NSFontAttributeName : font,
      NSLinkAttributeName :
          [NSURL URLWithString:@"chrometodayextension://footerButtonPressed"]
    };
    [_attributedText setAttributes:linkURLAttributes range:linkRange];

    [_attributedText setAttributes:hiddenAttributes range:_buttonRange];

    [_label setLinkTextAttributes:linkAttributes];

    [_label setAttributedText:_attributedText];
    [_label setTextAlignment:labelAlignment];

    if (showButton) {
      base::scoped_nsobject<NSMutableAttributedString> buttonAttributedTitle(
          [[NSMutableAttributedString alloc] initWithString:buttonText]);
      [paragraphStyle setLineSpacing:0];
      NSDictionary* buttonAttributes = @{
        NSParagraphStyleAttributeName : paragraphStyle,
        NSFontAttributeName : font,
        NSForegroundColorAttributeName : ui_util::TextColor(),
        NSBackgroundColorAttributeName : [UIColor clearColor],
      };
      [buttonAttributedTitle setAttributes:buttonAttributes
                                     range:NSMakeRange(0, [buttonText length])];
      [_activationButton setAttributedTitle:buttonAttributedTitle
                                   forState:UIControlStateNormal];
    }
  }
  return self;
}

- (CGRect)frameOfTextRange:(NSRange)range {
  // Temporary set editable flag to access the |UITextInput firstRectForRange:|
  // method.
  BOOL editableState = [_label isEditable];
  [_label setEditable:YES];
  UITextPosition* beginning = [_label beginningOfDocument];
  UITextPosition* start =
      [_label positionFromPosition:beginning offset:range.location];
  UITextPosition* end = [_label positionFromPosition:start offset:range.length];
  UITextRange* textRange = [_label textRangeFromPosition:start toPosition:end];
  CGRect textFrame = [_label firstRectForRange:textRange];
  textFrame = [_label convertRect:textFrame fromView:[_label textInputView]];
  [_label setEditable:editableState];
  return textFrame;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  if (self.frame.size.width != _currentWidth) {
    _currentWidth = self.frame.size.width;
    [self invalidateIntrinsicContentSize];
  }

  if (_activationButton) {
    CGRect textFrame = [self frameOfTextRange:_buttonRange];
    CGRect buttonFrame = [self convertRect:textFrame fromView:_label];
    [_buttonTopConstraint setConstant:buttonFrame.origin.y];
    [_buttonLeftConstraint setConstant:buttonFrame.origin.x];
    [_buttonWidthConstraint setConstant:buttonFrame.size.width];
    [_buttonHeightConstraint setConstant:buttonFrame.size.height];
  }
}

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange {
  _linkBlock.get()();
  return NO;
}

- (void)buttonPressed:(id)sender {
  _buttonBlock.get()();
}

- (CGSize)intrinsicContentSize {
  return [self sizeThatFits:CGSizeMake(_currentWidth, CGFLOAT_MAX)];
}

- (CGSize)sizeThatFits:(CGSize)size {
  if (![_attributedText length])
    return CGSizeMake(size.width,
                      MIN(_insets.top + _insets.bottom, size.height));

  // -sizeThatFits: doesn't behaves properly with UITextView.
  // Therefore, we need to measure text size using
  // -boundingRectWithSize:options:context:.
  CGSize constraints =
      CGSizeMake(size.width - _insets.left - _insets.right, CGFLOAT_MAX);
  CGRect bounding = [_attributedText
      boundingRectWithSize:constraints
                   options:static_cast<NSStringDrawingOptions>(
                               NSStringDrawingUsesLineFragmentOrigin |
                               NSStringDrawingUsesFontLeading)
                   context:nil];
  return CGSizeMake(
      size.width,
      MIN(bounding.size.height + _insets.top + _insets.bottom, size.height));
}

@end

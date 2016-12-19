// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/transparent_link_button.h"

#include "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#import "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "base/strings/sys_string_conversions.h"
#include "url/gurl.h"

// Minimum tap area dimension, as specified by Apple guidelines.
const CGFloat kLinkTapAreaMinimum = 44.0;

namespace {
// The corner radius of the highlight view.
const CGFloat kHighlightViewCornerRadius = 2.0;
// The alpha of the highlight view's background color (base color is black).
const CGFloat kHighlightViewBackgroundAlpha = 0.25;
}

@interface TransparentLinkButton () {
  // Backing objects for properties of the same name.
  base::scoped_nsobject<UIView> _highlightView;
  base::WeakNSObject<TransparentLinkButton> _previousLinkButton;
  base::WeakNSObject<TransparentLinkButton> _nextLinkButton;
}

// The link frame passed upon initialization.
@property(nonatomic, readonly) CGRect linkFrame;

// Semi-transparent overlay that is shown to highlight the link text when the
// button's highlight state is set to YES.
@property(nonatomic, readonly) UIView* highlightView;

// Links that span multiple lines require more than one TransparentLinkButton.
// These properties are used to populate the highlight state from one button to
// the other buttons corresponding with the same link.
@property(nonatomic, weak) TransparentLinkButton* previousLinkButton;
@property(nonatomic, weak) TransparentLinkButton* nextLinkButton;

// Designated initializer.  |linkFrame| is the frame of the link text; this may
// differ from the actual frame of the resulting TransparentLinkButton, which is
// guaranteed to be at least |kLinkTapAreaMinimum| in each dimension.  |URL| is
// the URL for the associated link.
- (instancetype)initWithLinkFrame:(CGRect)linkFrame
                              URL:(const GURL&)URL NS_DESIGNATED_INITIALIZER;

// Sets the properties, propogating state to its adjacent link buttons.
// |sender| is the TransparentLinkButon whose state is being propogated to
// |self|.
- (void)setHighlighted:(BOOL)highlighted sender:(TransparentLinkButton*)sender;
- (void)setSelected:(BOOL)selected sender:(TransparentLinkButton*)sender;

// Updates the appearance of |highlightView| based on highlighted/selected
// state.
- (void)updateHighlightView;

@end

@implementation TransparentLinkButton

@synthesize URL = _URL;
@synthesize debug = _debug;
@synthesize linkFrame = _linkFrame;

- (instancetype)initWithLinkFrame:(CGRect)linkFrame URL:(const GURL&)URL {
  CGFloat linkHeightExpansion =
      MAX(0, (kLinkTapAreaMinimum - linkFrame.size.height) / 2.0);
  CGFloat linkWidthExpansion =
      MAX(0, (kLinkTapAreaMinimum - linkFrame.size.width) / 2.0);
  // Expand the frame as necessary to meet the minimum tap area dimensions.
  CGRect frame =
      CGRectInset(linkFrame, -linkWidthExpansion, -linkHeightExpansion);
  if ((self = [super initWithFrame:frame])) {
    DCHECK(URL.is_valid());
    self.contentEdgeInsets =
        UIEdgeInsetsMake(linkHeightExpansion, linkWidthExpansion,
                         linkHeightExpansion, linkWidthExpansion);
    self.backgroundColor = [UIColor clearColor];
    _linkFrame = linkFrame;
    _URL = URL;
    // These buttons are positioned absolutely based on the the position of
    // regions of text that is already correctly aligned for RTL if necessary.
    self.semanticContentAttribute = UISemanticContentAttributeSpatial;
  }
  return self;
}

#pragma mark - Accessors

- (void)setPreviousLinkButton:(TransparentLinkButton*)previousLinkButton {
  _previousLinkButton.reset(previousLinkButton);
}

- (TransparentLinkButton*)previousLinkButton {
  return _previousLinkButton.get();
}

- (void)setNextLinkButton:(TransparentLinkButton*)nextLinkButton {
  _nextLinkButton.reset(nextLinkButton);
}

- (TransparentLinkButton*)nextLinkButton {
  return _nextLinkButton.get();
}

- (void)setDebug:(BOOL)debug {
  _debug = debug;
  self.layer.borderWidth = _debug ? 1.0 : 0.0;
  self.layer.borderColor =
      _debug ? [UIColor greenColor].CGColor : [UIColor clearColor].CGColor;
  self.backgroundColor = _debug ? [UIColor redColor] : [UIColor clearColor];
  self.alpha = _debug ? 0.15 : 1.0;
}

- (UIView*)highlightView {
  if (!_highlightView) {
    CGRect linkFrame =
        [self convertRect:self.linkFrame fromView:self.superview];
    linkFrame = CGRectInset(linkFrame, -kHighlightViewCornerRadius, 0);
    _highlightView.reset([[UIView alloc] initWithFrame:linkFrame]);
    [_highlightView
        setBackgroundColor:[UIColor
                               colorWithWhite:0.0
                                        alpha:kHighlightViewBackgroundAlpha]];
    [_highlightView layer].cornerRadius = kHighlightViewCornerRadius;
    [_highlightView setClipsToBounds:YES];
    [self addSubview:_highlightView];
  }
  return _highlightView.get();
}

- (void)setHighlighted:(BOOL)highlighted {
  [self setHighlighted:highlighted sender:nil];
}

- (void)setSelected:(BOOL)selected {
  [self setSelected:selected sender:nil];
}

#pragma mark -

+ (NSArray*)buttonsForLinkFrames:(NSArray*)linkFrames
                             URL:(const GURL&)URL
              accessibilityLabel:(NSString*)label {
  if (!linkFrames.count)
    return @[];
  base::scoped_nsobject<NSMutableArray> buttons(
      [[NSMutableArray alloc] initWithCapacity:linkFrames.count]);
  for (NSValue* linkFrameValue in linkFrames) {
    CGRect linkFrame = [linkFrameValue CGRectValue];
    base::scoped_nsobject<TransparentLinkButton> button(
        [[TransparentLinkButton alloc] initWithLinkFrame:linkFrame URL:URL]);
    TransparentLinkButton* previousButton = [buttons lastObject];
    previousButton.nextLinkButton = button;
    [button setPreviousLinkButton:previousButton];
    // Make buttons not accessible by default, but provide label for tests.
    [button setIsAccessibilityElement:NO];
    [button setAccessibilityLabel:label];
    [buttons addObject:button];
  }
  // Make the first button accessible.
  [buttons[0] setIsAccessibilityElement:YES];
  return [NSArray arrayWithArray:buttons];
}

- (void)setHighlighted:(BOOL)highlighted sender:(TransparentLinkButton*)sender {
  [super setHighlighted:highlighted];
  if (self.previousLinkButton != sender)
    [self.previousLinkButton setHighlighted:highlighted sender:self];
  if (self.nextLinkButton != sender)
    [self.nextLinkButton setHighlighted:highlighted sender:self];
  [self updateHighlightView];
}

- (void)setSelected:(BOOL)selected sender:(TransparentLinkButton*)sender {
  [super setSelected:selected];
  if (self.previousLinkButton != sender)
    [self.previousLinkButton setSelected:selected sender:self];
  if (self.nextLinkButton != sender)
    [self.nextLinkButton setSelected:selected sender:self];
  [self updateHighlightView];
}

- (void)updateHighlightView {
  self.highlightView.hidden = !self.highlighted && !self.selected;
}

@end

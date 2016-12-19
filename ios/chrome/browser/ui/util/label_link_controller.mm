// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/label_link_controller.h"

#include <map>
#include <vector>

#include "base/ios/ios_util.h"
#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/util/label_observer.h"
#import "ios/chrome/browser/ui/util/text_region_mapper.h"
#import "ios/chrome/browser/ui/util/transparent_link_button.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#pragma mark - LinkLayout

// Object encapsulating the range of a link and the frames corresponding with
// that range.
@interface LinkLayout : NSObject {
  // Backing objects for properties of same name.
  base::scoped_nsobject<NSArray> _frames;
}

// Designated initializer.
- (instancetype)initWithRange:(NSRange)range NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The range passed on initialization.
@property(nonatomic, readonly) NSRange range;

// The frames calculated for |_range|.
@property(nonatomic, retain) NSArray* frames;

@end

@implementation LinkLayout

@synthesize range = _range;

- (instancetype)initWithRange:(NSRange)range {
  if ((self = [super init])) {
    DCHECK_NE(range.location, static_cast<NSUInteger>(NSNotFound));
    DCHECK_NE(range.length, 0U);
    _range = range;
  }
  return self;
}

#pragma mark - Accessors

- (void)setFrames:(NSArray*)frames {
  _frames.reset([frames retain]);
}

- (NSArray*)frames {
  return _frames.get();
}

@end

#pragma mark - LabelLinkController

@interface LabelLinkController ()
// Private property exposed publically in testing interface.
@property(nonatomic, assign) Class textMapperClass;

// The original attributed text set on the label.  This may be different from
// the label's |attributedText| property, as additional style attributes may be
// introduced for links.
@property(nonatomic, readonly) NSAttributedString* originalLabelText;

// The array of TransparentLinkButtons inserted above the label.
@property(nonatomic, readonly) NSArray* linkButtons;

// Adds LabelObserverActions to the LabelObserver corresponding to |_label|.
- (void)addLabelObserverActions;

// Clears all defined links and any data associated with them. Update the
// original attributed text from the controlled label.
- (void)reset;

// Handle a change to the label that changes the positioning of glyphs but not
// any styling of those glyphs. Forces a recomputation of the tap regions, and
// recreates any tap buttons.
- (void)labelLayoutInvalidated;

// Handle a change to the label that changes the glyph style. This forces all of
// the link-specific styling applied by this class to be regenerated (which
// itself will again re-trigger this method), and because any kind of style
// change may alter the position of glyphs, this forces a layout invalidation.
- (void)labelStyleInvalidated;

// Updates the attributed string content of the controlled label to
// have the designated link colors and styles.
// No-op if no links are defined.
- (void)updateStyles;

// If the controlled label's bounds have changed from the last time tap rects
// were updated, determine which regions in the label should be tappable.
- (void)updateTapRects;

// Creates a new text mapper instance with the current label bounds and
// attributed text.
- (void)resetTextMapper;

// Clear any tap buttons that have been created, removing them from their
// superview if necessary.
- (void)clearTapButtons;

// Updates the tap buttons as detailed below. This method is called every time
// tap rects are updated, as well as every time |_label|'s superview changes.
// If there are no tap buttons defined, but there are known tap rects, and
// |_label| has a superview, then tap buttons are created and added to that
// view.
// If there are tap buttons, and |_label| has no superview, then the tap buttons
// are cleared.
// If there are tap buttons, but they are not subviews of |_label|'s superview
// (if _label's superview has changed since the buttons were created), then
// the tap buttons are migrated into the new superview.
- (void)updateTapButtons;

@end

@implementation LabelLinkController {
  // Ivars immutable for the lifetime of the object.
  base::mac::ScopedBlock<ProceduralBlockWithURL> _action;
  base::WeakNSObject<UILabel> _label;  // weak
  base::scoped_nsobject<UITapGestureRecognizer> _linkTapRecognizer;

  // Ivas backing properties.
  base::scoped_nsobject<UIColor> _linkColor;
  base::scoped_nsobject<UIFont> _linkFont;

  // Ivars that reset when label text changes.
  base::scoped_nsobject<NSMutableDictionary> _layoutsForURLs;
  base::scoped_nsobject<NSAttributedString> _originalLabelText;
  CGRect _lastLabelFrame;

  // Ivars that reset when text or bounds change.
  base::scoped_nsprotocol<id<TextRegionMapper>> _textMapper;

  // Internal tracking.
  BOOL _justUpdatedStyles;
  base::scoped_nsobject<NSMutableArray> _linkButtons;
}

@synthesize showTapAreas = _showTapAreas;
@synthesize textMapperClass = _textMapperClass;
@synthesize linkUnderlineStyle = _linkUnderlineStyle;

- (instancetype)initWithLabel:(UILabel*)label
                       action:(ProceduralBlockWithURL)action {
  if ((self = [super init])) {
    DCHECK(label);
    _label.reset(label);
    _action.reset(action, base::scoped_policy::RETAIN);
    _linkUnderlineStyle = NSUnderlineStyleNone;
    [self reset];

    [self addLabelObserverActions];

    self.textMapperClass = [CoreTextRegionMapper class];
    _linkButtons.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (NSAttributedString*)originalLabelText {
  return _originalLabelText.get();
}

- (NSArray*)linkButtons {
  return _linkButtons.get();
}

- (void)addLabelObserverActions {
  LabelObserver* observer = [LabelObserver observerForLabel:_label];
  base::WeakNSObject<LabelLinkController> weakSelf(self);
  [observer addStyleChangedAction:^(UILabel* label) {
    // One of the style properties has been changed, which will silently
    // update the label's attributedText.
    if (!weakSelf)
      return;
    base::scoped_nsobject<LabelLinkController> strongSelf([weakSelf retain]);
    [strongSelf labelStyleInvalidated];
  }];
  [observer addTextChangedAction:^(UILabel* label) {
    if (!weakSelf)
      return;
    base::scoped_nsobject<LabelLinkController> strongSelf([weakSelf retain]);
    NSString* originalText = [[strongSelf originalLabelText] string];
    if ([label.text isEqualToString:originalText]) {
      // The actual text of the label didn't change, so this was a change to
      // the string attributes only.
      [strongSelf labelStyleInvalidated];
    } else {
      // The label text has changed, so start everything from scratch.
      [strongSelf reset];
    }
  }];
  [observer addLayoutChangedAction:^(UILabel* label) {
    if (!weakSelf)
      return;
    base::scoped_nsobject<LabelLinkController> strongSelf([weakSelf retain]);
    [strongSelf labelLayoutInvalidated];
    NSArray* linkButtons = [strongSelf linkButtons];
    // If this layout change corresponds to |label|'s moving to a new superview,
    // update the tap buttons so that they are inserted above |label| in the new
    // hierarchy.
    if (linkButtons.count && label.superview != [linkButtons[0] superview])
      [strongSelf updateTapButtons];
  }];
}

- (void)dealloc {
  [self clearTapButtons];
  [super dealloc];
}

- (void)addLinkWithRange:(NSRange)range url:(GURL)url {
  DCHECK(url.is_valid());
  if (!_layoutsForURLs)
    _layoutsForURLs.reset([[NSMutableDictionary alloc] init]);
  NSURL* key = net::NSURLWithGURL(url);
  base::scoped_nsobject<LinkLayout> layout(
      [[LinkLayout alloc] initWithRange:range]);
  [_layoutsForURLs setObject:layout forKey:key];
  [self updateStyles];
}

- (UIColor*)linkColor {
  return _linkColor.get();
}

- (void)setLinkColor:(UIColor*)linkColor {
  _linkColor.reset([linkColor copy]);
  [self updateStyles];
}

- (void)setLinkUnderlineStyle:(NSUnderlineStyle)underlineStyle {
  _linkUnderlineStyle = underlineStyle;
  [self updateStyles];
}

- (UIFont*)linkFont {
  return _linkFont.get();
}

- (void)setLinkFont:(UIFont*)linkFont {
  _linkFont.reset([linkFont retain]);
  [self updateStyles];
}

- (void)setShowTapAreas:(BOOL)showTapAreas {
#ifndef NDEBUG
  for (TransparentLinkButton* button in _linkButtons.get()) {
    button.debug = showTapAreas;
  }
#endif  // NDEBUG
  _showTapAreas = showTapAreas;
}

#pragma mark - internal methods

- (void)reset {
  _originalLabelText.reset([[_label attributedText] copy]);
  _textMapper.reset();
  _lastLabelFrame = CGRectZero;
  _layoutsForURLs.reset();
}

- (void)labelLayoutInvalidated {
  _textMapper.reset();
  [self updateTapRects];
}

- (void)labelStyleInvalidated {
  // If the style invalidation was triggered by this class updating link styles,
  // then the original label text is still correct, but the tap rects still need
  // to be updated. Otherwise, update the original label text, and then update
  // styles. This will set |_justUpdatedStyles| and trigger another call to
  // this method via KVO.
  if (_justUpdatedStyles) {
    // TODO(crbug.com/664648): Remove _justUpdatedStyles due to bug that
    // prevents proper style updates after successive label format changes.
    _justUpdatedStyles = NO;
  } else if (![_originalLabelText isEqual:[_label attributedText]]) {
    _originalLabelText.reset([[_label attributedText] copy]);
    [self updateStyles];
  }
  _lastLabelFrame = CGRectZero;
  [self labelLayoutInvalidated];
}

- (void)updateStyles {
  if (![_layoutsForURLs count])
    return;

  __block base::scoped_nsobject<NSMutableAttributedString> labelText(
      [_originalLabelText mutableCopy]);
  [_layoutsForURLs enumerateKeysAndObjectsUsingBlock:^(
                       NSURL* key, LinkLayout* layout, BOOL* stop) {
    if (_linkColor) {
      [labelText addAttribute:NSForegroundColorAttributeName
                        value:_linkColor
                        range:layout.range];
    }
    if (_linkUnderlineStyle != NSUnderlineStyleNone) {
      [labelText addAttribute:NSUnderlineStyleAttributeName
                        value:@(_linkUnderlineStyle)
                        range:layout.range];
    }
    if (_linkFont) {
      [labelText addAttribute:NSFontAttributeName
                        value:_linkFont
                        range:layout.range];
    }
  }];
  _justUpdatedStyles = YES;
  [_label setAttributedText:labelText];
  _textMapper.reset();
}

- (void)updateTapRects {
  // Don't update if the label hasn't changed size or position.
  if (CGRectEqualToRect([_label frame], _lastLabelFrame))
    return;
  // Don't update if there are no links.
  if (![_layoutsForURLs count])
    return;

  _lastLabelFrame = [_label frame];
  [self clearTapButtons];

  // If the label bounds are zero in either dimension, no rects are possible.
  if (0.0 == _lastLabelFrame.size.width || 0.0 == _lastLabelFrame.size.height)
    return;

  if (!_textMapper)
    [self resetTextMapper];

  for (LinkLayout* layout in [_layoutsForURLs allValues]) {
    base::scoped_nsobject<NSMutableArray> frames([[NSMutableArray alloc] init]);
    NSArray* rects = [_textMapper rectsForRange:layout.range];
    for (NSUInteger rectIdx = 0; rectIdx < [rects count]; ++rectIdx) {
      CGRect frame = [rects[rectIdx] CGRectValue];
      frame = [[_label superview] convertRect:frame fromView:_label];
      [frames addObject:[NSValue valueWithCGRect:frame]];
    }
    layout.frames = frames;
  }
  [self updateTapButtons];
}

- (void)resetTextMapper {
  DCHECK([self.textMapperClass conformsToProtocol:@protocol(TextRegionMapper)]);
  _textMapper.reset([[self.textMapperClass alloc]
      initWithAttributedString:[_label attributedText]
                        bounds:[_label bounds]]);
}

- (void)clearTapButtons {
  for (TransparentLinkButton* button in _linkButtons.get()) {
    [button removeFromSuperview];
  }
  [_linkButtons removeAllObjects];
}

- (void)updateTapButtons {
  // If the label has no superview, clear any existing buttons.
  if (![_label superview]) {
    [self clearTapButtons];
    return;
  } else if ([_linkButtons count]) {
    // If the buttons are currently in some view other than the label's
    // superview, repatriate them.
    if (base::mac::ObjCCast<TransparentLinkButton>(_linkButtons[0]).superview !=
        [_label superview]) {
      for (TransparentLinkButton* button in _linkButtons.get()) {
        CGRect newFrame =
            [[_label superview] convertRect:button.frame fromView:button];
        [[_label superview] insertSubview:button aboveSubview:_label];
        button.frame = newFrame;
      }
    }
  }
  // If there are no buttons, make some and put them in the label's superview.
  if (![_linkButtons count] && _layoutsForURLs) {
    [_layoutsForURLs enumerateKeysAndObjectsUsingBlock:^(
                         NSURL* key, LinkLayout* layout, BOOL* stop) {
      GURL URL = net::GURLWithNSURL(key);
      NSString* accessibilityLabel =
          [[_label text] substringWithRange:layout.range];
      NSArray* buttons =
          [TransparentLinkButton buttonsForLinkFrames:layout.frames
                                                  URL:URL
                                   accessibilityLabel:accessibilityLabel];
      for (TransparentLinkButton* button in buttons) {
#ifndef NDEBUG
        button.debug = self.showTapAreas;
#endif  // NDEBUG
        [button addTarget:self
                      action:@selector(linkButtonTapped:)
            forControlEvents:UIControlEventTouchUpInside];
        [[_label superview] insertSubview:button aboveSubview:_label];
        [_linkButtons addObject:button];
      }
    }];
  }
}

#pragma mark - Tap Handlers

- (void)linkButtonTapped:(id)sender {
  TransparentLinkButton* button =
      base::mac::ObjCCast<TransparentLinkButton>(sender);
  _action.get()(button.URL);
}

#pragma mark - Test facilitators

- (NSArray*)tapRectsForURL:(GURL)url {
  NSURL* key = net::NSURLWithGURL(url);
  LinkLayout* layout = [_layoutsForURLs objectForKey:key];
  return layout.frames;
}

- (void)tapLabelAtPoint:(CGPoint)point {
  [_layoutsForURLs enumerateKeysAndObjectsUsingBlock:^(
                       NSURL* key, LinkLayout* layout, BOOL* stop) {
    for (NSValue* frameValue in layout.frames) {
      CGRect frame = [frameValue CGRectValue];
      if (CGRectContainsPoint(frame, point)) {
        _action.get()(net::GURLWithNSURL(key));
        *stop = YES;
        break;
      }
    }
  }];
}

@end

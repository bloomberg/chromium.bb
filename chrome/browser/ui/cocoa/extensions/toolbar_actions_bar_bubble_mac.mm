// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/toolbar_actions_bar_bubble_mac.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "third_party/skia/include/core/SkColor.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/hover_button.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/native_theme/native_theme.h"

namespace {
BOOL g_animations_enabled = false;
}

@class ExtensionMessageBubbleButton;

@interface ToolbarActionsBarBubbleMac ()

// Handles the notification that the window will close.
- (void)windowWillClose:(NSNotification*)notification;

// Creates and returns an NSAttributed string with the specified size and
// alignment.
- (NSAttributedString*)attributedStringWithString:(const base::string16&)string
                                         fontSize:(CGFloat)fontSize
                                        alignment:(NSTextAlignment)alignment;

// Creates an NSTextField with the given string, size, and alignment, and adds
// it to the window.
- (NSTextField*)addTextFieldWithString:(const base::string16&)string
                              fontSize:(CGFloat)fontSize
                             alignment:(NSTextAlignment)alignment;

// Creates an ExtensionMessagebubbleButton the given string id, and adds it to
// the window.
- (ExtensionMessageBubbleButton*) addButtonWithString:
    (const base::string16&)string
    isPrimary:(BOOL)isPrimary;

// Initializes the bubble's content.
- (void)layout;

// Handles a button being clicked.
- (void)onButtonClicked:(id)sender;

@end

@interface ExtensionMessageBubbleButton : HoverButton {
  BOOL isPrimary_;
}

// Draws with a blue background and white text.
- (void)drawRect:(NSRect)rect;

@property(nonatomic) BOOL isPrimary;

@end

@implementation ToolbarActionsBarBubbleMac

@synthesize actionButton = actionButton_;
@synthesize itemList = itemList_;
@synthesize dismissButton = dismissButton_;
@synthesize learnMoreButton = learnMoreButton_;

- (id)initWithParentWindow:(NSWindow*)parentWindow
               anchorPoint:(NSPoint)anchorPoint
                  delegate:(scoped_ptr<ToolbarActionsBarBubbleDelegate>)
                               delegate {
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:anchorPoint])) {
    acknowledged_ = NO;
    [window setInfoBubbleCanBecomeKeyWindow:NO];
    delegate_ = delegate.Pass();

    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [[self bubble] setAlignment:info_bubble::kAlignRightEdgeToAnchorEdge];
    [[self bubble] setArrowLocation:info_bubble::kNoArrow];
    [[self bubble] setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground))];

    if (!g_animations_enabled)
      [window setAllowedAnimations:info_bubble::kAnimateNone];

    [self layout];
  }
  return self;
}

+ (void)setAnimationEnabledForTesting:(BOOL)enabled {
  g_animations_enabled = enabled;
}

- (IBAction)showWindow:(id)sender {
  delegate_->OnBubbleShown();
  [super showWindow:sender];
}

// Private /////////////////////////////////////////////////////////////////////

- (void)windowWillClose:(NSNotification*)notification {
  if (!acknowledged_) {
    delegate_->OnBubbleClosed(
        ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS);
    acknowledged_ = YES;
  }
  [super windowWillClose:notification];
}

- (NSAttributedString*)attributedStringWithString:(const base::string16&)string
                                         fontSize:(CGFloat)fontSize
                                        alignment:(NSTextAlignment)alignment {
  NSString* cocoaString = base::SysUTF16ToNSString(string);
  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:alignment];
  NSDictionary* attributes = @{
    NSFontAttributeName : [NSFont systemFontOfSize:fontSize],
    NSForegroundColorAttributeName :
        [NSColor colorWithCalibratedWhite:0.2 alpha:1.0],
    NSParagraphStyleAttributeName : paragraphStyle.get()
  };
  return [[[NSAttributedString alloc] initWithString:cocoaString
                                          attributes:attributes] autorelease];
}

- (NSTextField*)addTextFieldWithString:(const base::string16&)string
                              fontSize:(CGFloat)fontSize
                             alignment:(NSTextAlignment)alignment {
  NSAttributedString* attributedString =
      [self attributedStringWithString:string
                              fontSize:fontSize
                             alignment:alignment];

  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [textField setEditable:NO];
  [textField setBordered:NO];
  [textField setDrawsBackground:NO];
  [textField setAttributedStringValue:attributedString];
  [[[self window] contentView] addSubview:textField];
  [textField sizeToFit];
  return textField.autorelease();
}

- (ExtensionMessageBubbleButton*)addButtonWithString:
    (const base::string16&)string
    isPrimary:(BOOL)isPrimary {
  ExtensionMessageBubbleButton* button =
      [[ExtensionMessageBubbleButton alloc] initWithFrame:NSZeroRect];
  NSAttributedString* buttonString =
      [self attributedStringWithString:string
                              fontSize:13.0
                             alignment:NSCenterTextAlignment];
  [button setAttributedTitle:buttonString];
  [button setIsPrimary:isPrimary];
  [[button cell] setBordered:NO];
  [button setTarget:self];
  [button setAction:@selector(onButtonClicked:)];
  [[[self window] contentView] addSubview:button];
  [button sizeToFit];
  return button;
}

- (void)layout {
  // We first construct the different pieces of the bubble (the heading, the
  // content, and the button), and size them appropriately.
  NSTextField* heading =
      [self addTextFieldWithString:delegate_->GetHeadingText()
                          fontSize:13.0
                         alignment:NSLeftTextAlignment];
  NSSize headingSize = [heading frame].size;

  NSTextField* content =
      [self addTextFieldWithString:delegate_->GetBodyText()
                          fontSize:12.0
                         alignment:NSLeftTextAlignment];
  CGFloat newWidth = headingSize.width + 50;
  [content setFrame:NSMakeRect(0, 0, newWidth, 0)];
  // The content should have the same (max) width as the heading, which means
  // the text will most likely wrap.
  NSSize contentSize = NSMakeSize(newWidth,
                                  [GTMUILocalizerAndLayoutTweaker
                                       sizeToFitFixedWidthTextField:content]);

  const CGFloat kItemListIndentation = 10.0;
  base::string16 itemListStr = delegate_->GetItemListText();
  NSSize itemListSize;
  if (!itemListStr.empty()) {
    itemList_ =
        [self addTextFieldWithString:itemListStr
                            fontSize:12.0
                           alignment:NSLeftTextAlignment];
    CGFloat listWidth = newWidth - kItemListIndentation;
    [itemList_ setFrame:NSMakeRect(0, 0, listWidth, 0)];
    itemListSize = NSMakeSize(listWidth,
                              [GTMUILocalizerAndLayoutTweaker
                                   sizeToFitFixedWidthTextField:itemList_]);
  }

  base::string16 learnMore = delegate_->GetLearnMoreButtonText();
  NSSize learnMoreSize = NSZeroSize;
  if (!learnMore.empty()) {  // The "learn more" link is optional.
    NSAttributedString* learnMoreString =
        [self attributedStringWithString:learnMore
                                fontSize:13.0
                               alignment:NSLeftTextAlignment];
    learnMoreButton_ =
        [[HyperlinkButtonCell buttonWithString:learnMoreString.string] retain];
    [learnMoreButton_ setTarget:self];
    [learnMoreButton_ setAction:@selector(onButtonClicked:)];
    [[[self window] contentView] addSubview:learnMoreButton_];
    [learnMoreButton_ sizeToFit];
    learnMoreSize = NSMakeSize(NSWidth([learnMoreButton_ frame]),
                               NSHeight([learnMoreButton_ frame]));
  }

  // The buttons' sizes will only account for the text by default, so pad them a
  // bit to make it look good.
  const CGFloat kButtonHorizontalPadding = 40.0;
  const CGFloat kButtonVerticalPadding = 20.0;

  base::string16 actionStr = delegate_->GetActionButtonText();
  bool hasAction = !actionStr.empty();

  base::string16 cancelStr = delegate_->GetDismissButtonText();
  NSSize dismissButtonSize = NSZeroSize;
  if (!cancelStr.empty()) {  // A cancel/dismiss button is optional.
    dismissButton_ = [self addButtonWithString:cancelStr
                                     isPrimary:!hasAction];
    dismissButtonSize =
        NSMakeSize(NSWidth([dismissButton_ frame]) + kButtonHorizontalPadding,
                   NSHeight([dismissButton_ frame]) + kButtonVerticalPadding);
  }

  NSSize actionButtonSize = NSZeroSize;
  if (hasAction) {  // A cancel/dismiss button is optional.
    actionButton_ = [self addButtonWithString:actionStr
                                     isPrimary:YES];
    actionButtonSize =
        NSMakeSize(NSWidth([actionButton_ frame]) + kButtonHorizontalPadding,
                   NSHeight([actionButton_ frame]) + kButtonVerticalPadding);
  }

  const CGFloat kHorizontalPadding = 15.0;
  const CGFloat kVerticalPadding = 10.0;

  // Next, we set frame for all the different pieces of the bubble, from bottom
  // to top.
  CGFloat windowWidth = newWidth + kHorizontalPadding * 2;

  CGFloat currentHeight = 0;
  CGFloat currentMaxWidth = windowWidth;
  if (actionButton_) {
    [actionButton_ setFrame:NSMakeRect(currentMaxWidth - actionButtonSize.width,
                                       currentHeight,
                                       actionButtonSize.width,
                                       actionButtonSize.height)];
    currentMaxWidth -= actionButtonSize.width;
  }
  if (dismissButton_) {
    [dismissButton_ setFrame:NSMakeRect(
        currentMaxWidth - dismissButtonSize.width,
        currentHeight,
        dismissButtonSize.width,
        dismissButtonSize.height)];
    currentMaxWidth -= dismissButtonSize.width;
  }
  currentHeight += actionButtonSize.height + kVerticalPadding;

  if (learnMoreButton_) {
    [learnMoreButton_ setFrame:NSMakeRect(kHorizontalPadding,
                                          currentHeight,
                                          learnMoreSize.width,
                                          learnMoreSize.height)];
    currentHeight += learnMoreSize.height + kVerticalPadding;
  }

  if (itemList_) {
    [itemList_ setFrame:NSMakeRect(kHorizontalPadding + kItemListIndentation,
                                   currentHeight,
                                   itemListSize.width,
                                   itemListSize.height)];
    currentHeight += itemListSize.height + kVerticalPadding;
  }

  [content setFrame:NSMakeRect(kHorizontalPadding,
                               currentHeight,
                               contentSize.width,
                               contentSize.height)];
  currentHeight += contentSize.height + kVerticalPadding;
  [heading setFrame:NSMakeRect(kHorizontalPadding,
                               currentHeight,
                               headingSize.width,
                               headingSize.height)];

  // Update window frame.
  NSRect windowFrame = [[self window] frame];
  NSSize windowSize =
      NSMakeSize(windowWidth,
                 currentHeight + headingSize.height + kVerticalPadding);
  // We need to convert the size to be in the window's coordinate system. Since
  // all we're doing is converting a size, and all views within a window share
  // the same size metrics, it's okay that the size calculation came from
  // multiple different views. Pick a view to convert it.
  windowSize = [heading convertSize:windowSize toView:nil];
  windowFrame.size = windowSize;
  [[self window] setFrame:windowFrame display:YES];
}

- (void)onButtonClicked:(id)sender {
  if (acknowledged_)
    return;
  ToolbarActionsBarBubbleDelegate::CloseAction action =
      ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE;
  if (learnMoreButton_ && sender == learnMoreButton_) {
    action = ToolbarActionsBarBubbleDelegate::CLOSE_LEARN_MORE;
  } else if (dismissButton_ && sender == dismissButton_) {
    action = ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS;
  } else {
    DCHECK_EQ(sender, actionButton_);
    action = ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE;
  }
  acknowledged_ = YES;
  delegate_->OnBubbleClosed(action);
  [self close];
}

@end

@implementation ExtensionMessageBubbleButton

@synthesize isPrimary = isPrimary_;

- (void)drawRect:(NSRect)rect {
  SkColor buttonColor;
  SkColor textColor;
  if (isPrimary_) {
    buttonColor = SkColorSetRGB(66, 133, 244);
    textColor = [self hoverState] == kHoverStateNone ?
        SkColorSetARGB(220, 255, 255, 255) : SK_ColorWHITE;
  } else {
    buttonColor = SK_ColorTRANSPARENT;
    textColor = [self hoverState] == kHoverStateNone ?
        SkColorSetARGB(220, 0, 0, 0) : SK_ColorBLACK;
  }
  NSRect bounds = [self bounds];
  NSAttributedString* title = [self attributedTitle];

  [gfx::SkColorToCalibratedNSColor(buttonColor) set];
  NSRectFillUsingOperation(bounds, NSCompositeSourceOver);

  base::scoped_nsobject<NSMutableAttributedString> selectedTitle(
      [[NSMutableAttributedString alloc] initWithAttributedString:title]);
  NSColor* selectedTitleColor =
      gfx::SkColorToCalibratedNSColor(textColor);
  [selectedTitle addAttribute:NSForegroundColorAttributeName
                        value:selectedTitleColor
                        range:NSMakeRange(0, [title length])];

  if (!isPrimary_) {
    [[NSColor darkGrayColor] setStroke];
    [NSBezierPath strokeRect:bounds];
  }
  [[self cell] drawTitle:selectedTitle.get()
               withFrame:bounds
                  inView:self];
}

@end

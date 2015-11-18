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
CGFloat kMinWidth = 320.0;
}

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
- (NSButton*)addButtonWithString:(const base::string16&)string;

// Initializes the bubble's content.
- (void)layout;

// Handles a button being clicked.
- (void)onButtonClicked:(id)sender;

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
    delegate_ = delegate.Pass();

    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [[self bubble] setAlignment:info_bubble::kAlignArrowToAnchor];
    [[self bubble] setArrowLocation:info_bubble::kTopRight];
    [[self bubble] setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground))];

    if (!g_animations_enabled)
      [window setAllowedAnimations:info_bubble::kAnimateNone];

    [self layout];

    [[self window] makeFirstResponder:
        (actionButton_ ? actionButton_ : dismissButton_)];
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

- (NSButton*)addButtonWithString:(const base::string16&)string {
  NSButton* button = [[NSButton alloc] initWithFrame:NSZeroRect];
  NSAttributedString* buttonString =
      [self attributedStringWithString:string
                              fontSize:13.0
                             alignment:NSCenterTextAlignment];
  [button setAttributedTitle:buttonString];
  [button setBezelStyle:NSRoundedBezelStyle];
  [button setTarget:self];
  [button setAction:@selector(onButtonClicked:)];
  [[[self window] contentView] addSubview:button];
  [button sizeToFit];
  return button;
}

- (void)layout {
  // First, construct the pieces of the bubble that have a fixed width: the
  // heading, and the button strip (the learn more link, the action button, and
  // the dismiss button).
  NSTextField* heading =
      [self addTextFieldWithString:delegate_->GetHeadingText()
                          fontSize:13.0
                         alignment:NSLeftTextAlignment];
  NSSize headingSize = [heading frame].size;

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

  base::string16 cancelStr = delegate_->GetDismissButtonText();
  NSSize dismissButtonSize = NSZeroSize;
  if (!cancelStr.empty()) {  // A cancel/dismiss button is optional.
    dismissButton_ = [self addButtonWithString:cancelStr];
    dismissButtonSize =
        NSMakeSize(NSWidth([dismissButton_ frame]),
                   NSHeight([dismissButton_ frame]));
  }

  base::string16 actionStr = delegate_->GetActionButtonText();
  NSSize actionButtonSize = NSZeroSize;
  if (!actionStr.empty()) {  // The action button is optional.
    actionButton_ = [self addButtonWithString:actionStr];
    actionButtonSize =
        NSMakeSize(NSWidth([actionButton_ frame]),
                   NSHeight([actionButton_ frame]));
  }

  DCHECK(actionButton_ || dismissButton_);
  CGFloat buttonStripHeight =
      std::max(actionButtonSize.height, dismissButtonSize.height);

  const CGFloat kButtonPadding = 5.0;
  CGFloat buttonStripWidth = 0;
  if (actionButton_)
    buttonStripWidth += actionButtonSize.width + kButtonPadding;
  if (dismissButton_)
    buttonStripWidth += dismissButtonSize.width + kButtonPadding;
  if (learnMoreButton_)
    buttonStripWidth += learnMoreSize.width + kButtonPadding;

  CGFloat headingWidth = headingSize.width;
  CGFloat windowWidth =
      std::max(std::max(kMinWidth, buttonStripWidth), headingWidth);

  NSTextField* content =
      [self addTextFieldWithString:delegate_->GetBodyText()
                          fontSize:12.0
                         alignment:NSLeftTextAlignment];
  [content setFrame:NSMakeRect(0, 0, windowWidth, 0)];
  // The content should have the same (max) width as the heading, which means
  // the text will most likely wrap.
  NSSize contentSize = NSMakeSize(windowWidth,
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
    CGFloat listWidth = windowWidth - kItemListIndentation;
    [itemList_ setFrame:NSMakeRect(0, 0, listWidth, 0)];
    itemListSize = NSMakeSize(listWidth,
                              [GTMUILocalizerAndLayoutTweaker
                                   sizeToFitFixedWidthTextField:itemList_]);
  }

  const CGFloat kHorizontalPadding = 15.0;
  const CGFloat kVerticalPadding = 10.0;

  // Next, we set frame for all the different pieces of the bubble, from bottom
  // to top.
  windowWidth += kHorizontalPadding * 2;
  CGFloat currentHeight = kVerticalPadding;
  CGFloat currentMaxWidth = windowWidth - kHorizontalPadding;
  if (actionButton_) {
    [actionButton_ setFrame:NSMakeRect(
        currentMaxWidth - actionButtonSize.width,
        currentHeight,
        actionButtonSize.width,
        actionButtonSize.height)];
    currentMaxWidth -= (actionButtonSize.width + kButtonPadding);
  }
  if (dismissButton_) {
    [dismissButton_ setFrame:NSMakeRect(
        currentMaxWidth - dismissButtonSize.width,
        currentHeight,
        dismissButtonSize.width,
        dismissButtonSize.height)];
    currentMaxWidth -= (dismissButtonSize.width + kButtonPadding);
  }
  if (learnMoreButton_) {
    CGFloat learnMoreHeight =
        currentHeight + (buttonStripHeight - learnMoreSize.height) / 2.0;
    [learnMoreButton_ setFrame:NSMakeRect(kHorizontalPadding,
                                          learnMoreHeight,
                                          learnMoreSize.width,
                                          learnMoreSize.height)];
  }
  // Buttons have some inherit padding of their own, so we don't need quite as
  // much space here.
  currentHeight += buttonStripHeight + kVerticalPadding / 2;

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
                 currentHeight + headingSize.height + kVerticalPadding * 2);
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

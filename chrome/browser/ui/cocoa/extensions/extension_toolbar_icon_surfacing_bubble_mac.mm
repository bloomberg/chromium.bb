// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_toolbar_icon_surfacing_bubble_mac.h"

#include "base/mac/foundation_util.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "third_party/skia/include/core/SkColor.h"
#import "ui/base/cocoa/hover_button.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"

@interface ExtensionToolbarIconSurfacingBubbleMac ()

// Handles the notification that the window will close.
- (void)windowWillClose:(NSNotification*)notification;

// Creates and returns an NSAttributed string with the specified size and
// alignment.
- (NSAttributedString*)attributedStringWithString:(int)stringId
                                         fontSize:(CGFloat)fontSize
                                        alignment:(NSTextAlignment)alignment;

// Creates an NSTextField with the given string, and adds it to the window.
- (NSTextField*)addTextFieldWithString:(NSAttributedString*)attributedString;

// Initializes the bubble's content.
- (void)layout;

// Handles the "ok" button being clicked.
- (void)onButtonClicked:(id)sender;

@end

@interface ExtensionToolbarIconSurfacingBubbleButton : HoverButton
// Draws with a blue background and white text.
- (void)drawRect:(NSRect)rect;
@end

@implementation ExtensionToolbarIconSurfacingBubbleMac

- (id)initWithParentWindow:(NSWindow*)parentWindow
               anchorPoint:(NSPoint)anchorPoint
                  delegate:(ToolbarActionsBarBubbleDelegate*)delegate {
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:anchorPoint])) {
    delegate_ = delegate;
    acknowledged_ = NO;
    [window setCanBecomeKeyWindow:NO];

    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [[self bubble] setAlignment:info_bubble::kAlignRightEdgeToAnchorEdge];
    [[self bubble] setArrowLocation:info_bubble::kNoArrow];
    [[self bubble] setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground))];

    [self layout];

    delegate_->OnToolbarActionsBarBubbleShown();
  }
  return self;
}

// Private /////////////////////////////////////////////////////////////////////

- (void)windowWillClose:(NSNotification*)notification {
  if (!acknowledged_) {
    delegate_->OnToolbarActionsBarBubbleClosed(
        ToolbarActionsBarBubbleDelegate::DISMISSED);
    acknowledged_ = YES;
  }
  [super windowWillClose:notification];
}

- (NSAttributedString*)attributedStringWithString:(int)stringId
                                         fontSize:(CGFloat)fontSize
                                        alignment:(NSTextAlignment)alignment {
  NSString* string = l10n_util::GetNSString(stringId);
  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:alignment];
  NSDictionary* attributes = @{
    NSFontAttributeName : [NSFont systemFontOfSize:fontSize],
    NSForegroundColorAttributeName :
        [NSColor colorWithCalibratedWhite:0.2 alpha:1.0],
    NSParagraphStyleAttributeName : paragraphStyle.get()
  };
  return [[[NSAttributedString alloc] initWithString:string
                                          attributes:attributes] autorelease];
}

- (NSTextField*)addTextFieldWithString:(NSAttributedString*)attributedString {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [textField setEditable:NO];
  [textField setBordered:NO];
  [textField setDrawsBackground:NO];
  [textField setAttributedStringValue:attributedString];
  [[[self window] contentView] addSubview:textField];
  return textField.autorelease();
}

- (void)layout {
  // We first construct the different pieces of the bubble (the heading, the
  // content, and the button), and size them appropriately.
  NSAttributedString* headingString =
      [self attributedStringWithString:IDS_EXTENSION_TOOLBAR_BUBBLE_HEADING
                              fontSize:13.0
                             alignment:NSLeftTextAlignment];
  NSTextField* heading = [self addTextFieldWithString:headingString];
  [heading sizeToFit];
  NSSize headingSize = [heading frame].size;

  NSAttributedString* contentString =
      [self attributedStringWithString:IDS_EXTENSION_TOOLBAR_BUBBLE_CONTENT
                              fontSize:12.0
                             alignment:NSLeftTextAlignment];
  NSTextField* content = [self addTextFieldWithString:contentString];
  [content setFrame:NSMakeRect(0, 0, headingSize.width, 0)];
  // The content should have the same (max) width as the heading, which means
  // the text will most likely wrap.
  NSSize contentSize = NSMakeSize(headingSize.width,
                                  [GTMUILocalizerAndLayoutTweaker
                                       sizeToFitFixedWidthTextField:content]);

  NSButton* button = [[ExtensionToolbarIconSurfacingBubbleButton alloc]
                         initWithFrame:NSZeroRect];
  NSAttributedString* buttonString =
      [self attributedStringWithString:IDS_EXTENSION_TOOLBAR_BUBBLE_OK
                              fontSize:13.0
                             alignment:NSCenterTextAlignment];
  [button setAttributedTitle:buttonString];
  [[button cell] setBordered:NO];
  [button setTarget:self];
  [button setAction:@selector(onButtonClicked:)];
  [[[self window] contentView] addSubview:button];
  [button sizeToFit];
  // The button's size will only account for the text by default, so pad it a
  // bit to make it look good.
  NSSize buttonSize = NSMakeSize(NSWidth([button frame]) + 40.0,
                                 NSHeight([button frame]) + 20.0);

  const CGFloat kHorizontalPadding = 15.0;
  const CGFloat kVerticalPadding = 10.0;

  // Next, we set frame for all the different pieces of the bubble, from bottom
  // to top.
  CGFloat windowWidth = headingSize.width + kHorizontalPadding * 2;

  CGFloat currentHeight = 0;
  [button setFrame:NSMakeRect(windowWidth - buttonSize.width,
                              currentHeight,
                              buttonSize.width,
                              buttonSize.height)];
  currentHeight += buttonSize.height + kVerticalPadding;
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
  if (!acknowledged_) {
    delegate_->OnToolbarActionsBarBubbleClosed(
        ToolbarActionsBarBubbleDelegate::ACKNOWLEDGED);
    acknowledged_ = YES;
    [self close];
  }
}

@end

@implementation ExtensionToolbarIconSurfacingBubbleButton

- (void)drawRect:(NSRect)rect {
  SkColor buttonColor = SkColorSetRGB(66, 133, 244);
  SkColor textColor = [self hoverState] == kHoverStateNone ?
      SkColorSetARGB(230, 255, 255, 255) : SK_ColorWHITE;
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

  [[self cell] drawTitle:selectedTitle.get()
               withFrame:bounds
                  inView:self];
}

@end

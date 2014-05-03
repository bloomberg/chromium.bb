// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/new_credit_card_bubble_cocoa.h"

#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/new_credit_card_bubble_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/native_theme/native_theme.h"

namespace {

const CGFloat kWrenchBubblePointOffsetY = 6;
const CGFloat kVerticalSpacing = 8;
const CGFloat kHorizontalSpacing = 4;
const CGFloat kInset = 20.0;
const CGFloat kTitleSpacing = 15;
const CGFloat kAnchorlessEndPadding = 20;
const CGFloat kAnchorlessTopPadding = 10;

}  // namespace

@interface NewCreditCardBubbleControllerCocoa : BaseBubbleController {
 @private
  // Controller that drives this bubble. Never NULL; outlives this class.
  autofill::NewCreditCardBubbleController* controller_;

  // Objective-C/C++ bridge. Owned by this class.
  scoped_ptr<autofill::NewCreditCardBubbleCocoa> bridge_;
}

// Designated initializer.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                controller:
                    (autofill::NewCreditCardBubbleController*)controller
                    bridge:(autofill::NewCreditCardBubbleCocoa*)bridge;

// Displays the bubble anchored at the specified |anchorPoint|.
- (void)showAtAnchor:(NSPoint)anchorPoint;

// Helper function to create an uneditable text field.
- (base::scoped_nsobject<NSTextField>)makeTextField;

// Helper function to set the appropriately styled details string.
- (void)setDetailsStringForField:(NSTextField*)details;

// Create and lay out all control elements inside the bubble.
- (void)performLayout;

// Delegate interface for clicks on "Learn more".
- (void)onLinkClicked:(id)sender;

@end


@implementation NewCreditCardBubbleControllerCocoa

- (id)initWithParentWindow:(NSWindow*)parentWindow
                controller:
                    (autofill::NewCreditCardBubbleController*)controller
                    bridge:(autofill::NewCreditCardBubbleCocoa*)bridge {
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
  bridge_.reset(bridge);
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    controller_ = controller;
    [window setCanBecomeKeyWindow:NO];

    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [[self bubble] setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground))];
    [self performLayout];
  }
  return self;
}

- (void)showAtAnchor:(NSPoint)anchorPoint {
  [self setAnchorPoint:anchorPoint];
  [[self bubble] setNeedsDisplay:YES];
  [self showWindow:nil];
}

- (base::scoped_nsobject<NSTextField>)makeTextField {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];

  return textField;
}

- (void)setDetailsStringForField:(NSTextField*)details {
  // Set linespacing for |details| to match line spacing used in the dialog.
  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setLineSpacing:0.5 * [[details font] pointSize]];

  NSString* description =
      base::SysUTF16ToNSString(controller_->CardDescription().description);
  base::scoped_nsobject<NSAttributedString> detailsString(
      [[NSAttributedString alloc]
           initWithString:description
               attributes:@{ NSParagraphStyleAttributeName : paragraphStyle }]);

  [details setAttributedStringValue:detailsString];
}

- (void)performLayout {
  base::scoped_nsobject<NSTextField> title([self makeTextField]);
  base::scoped_nsobject<NSImageView> icon([[NSImageView alloc]
      initWithFrame:NSZeroRect]);
  base::scoped_nsobject<NSTextField> name([self makeTextField]);
  base::scoped_nsobject<NSTextField> details([self makeTextField]);
  base::scoped_nsobject<NSButton> link([[HyperlinkButtonCell buttonWithString:
          base::SysUTF16ToNSString(controller_->LinkText())] retain]);

  [link setTarget:self];
  [link setAction:@selector(onLinkClicked:)];

  // Use uniform font across all controls (except title).
  [details setFont:[link font]];
  [name setFont:[link font]];
  [title setFont:[NSFont systemFontOfSize:15.0]];

  // Populate fields.
  const autofill::CreditCardDescription& card_desc =
      controller_->CardDescription();
  [title setStringValue:base::SysUTF16ToNSString(controller_->TitleText())];
  [icon setImage:card_desc.icon.AsNSImage()];
  [name setStringValue:base::SysUTF16ToNSString(card_desc.name)];
  [self setDetailsStringForField:details];

  // Size fields.
  CGFloat bubbleWidth = autofill::NewCreditCardBubbleView::kContentsWidth;
  [title sizeToFit];
  bubbleWidth = std::max(bubbleWidth, NSWidth([title frame]) + 2 * kInset);
  [icon setFrameSize:[[icon image] size]];
  [name sizeToFit];
  [details setFrameSize:[[details cell] cellSizeForBounds:
      NSMakeRect(0, 0, bubbleWidth - 2 * kInset, CGFLOAT_MAX)]];
  [link sizeToFit];

  // Layout fields.
  [link setFrameOrigin:NSMakePoint(kInset, kInset)];
  [details setFrameOrigin:NSMakePoint(
      kInset, NSMaxY([link frame]) + kVerticalSpacing)];
  [icon setFrameOrigin:NSMakePoint(
      kInset, NSMaxY([details frame]) + kVerticalSpacing)];
  [name setFrameOrigin:NSMakePoint(
      NSMaxX([icon frame]) + kHorizontalSpacing,
      NSMidY([icon frame]) - (NSHeight([name frame]) / 2.0))];
  [title setFrameOrigin:
      NSMakePoint(kInset, NSMaxY([icon frame]) + kTitleSpacing)];

  [[[self window] contentView] setSubviews:
      @[ title, icon, name, details, link ]];

  // Update window frame.
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height = NSMaxY([title frame]) + kInset;
  windowFrame.size.width = bubbleWidth;
  [[self window] setFrame:windowFrame display:NO];
}

- (void)onLinkClicked:(id)sender {
  controller_->OnLinkClicked();
}

@end


namespace autofill {

NewCreditCardBubbleCocoa::~NewCreditCardBubbleCocoa() {
  controller_->OnBubbleDestroyed();
}

void NewCreditCardBubbleCocoa::Show() {
  NSView* browser_view = controller_->web_contents()->GetNativeView();
  NSWindow* parent_window = [browser_view window];
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:parent_window];

  if (!bubbleController_)
    CreateCocoaController(parent_window);

  NSPoint anchor_point;
  NSView* anchor_view;
  if ([bwc isTabbedWindow]) {
    anchor_view = [[bwc toolbarController] wrenchButton];
    anchor_point = NSMakePoint(
        NSMidX([anchor_view bounds]),
        NSMinY([anchor_view bounds]) + kWrenchBubblePointOffsetY);
    [[bubbleController_ bubble] setArrowLocation:info_bubble::kTopRight];
    [[bubbleController_ bubble] setAlignment:info_bubble::kAlignArrowToAnchor];
  } else {
    anchor_view = browser_view;
    anchor_point = NSMakePoint(
        NSMaxX([browser_view bounds]) - kAnchorlessEndPadding,
        NSMaxY([browser_view bounds]) - kAnchorlessTopPadding);
    [[bubbleController_ bubble] setArrowLocation:info_bubble::kNoArrow];
    if (base::i18n::IsRTL()) {
      anchor_point.x = NSMaxX([anchor_view bounds]) - anchor_point.x;
      [[bubbleController_ bubble] setAlignment:
          info_bubble::kAlignLeftEdgeToAnchorEdge];
    } else {
      [[bubbleController_ bubble] setAlignment:
          info_bubble::kAlignRightEdgeToAnchorEdge];
    }
  }
  if ([anchor_view isFlipped])
    anchor_point.y = NSMaxY([anchor_view bounds]) - anchor_point.y;
  anchor_point = [anchor_view convertPoint:anchor_point toView:nil];

  NSRect frame = NSZeroRect;
  frame.origin = anchor_point;
  anchor_point = [parent_window convertBaseToScreen:anchor_point];
  [bubbleController_ showAtAnchor:anchor_point];
}

void NewCreditCardBubbleCocoa::Hide() {
  [bubbleController_ close];
}

// static
base::WeakPtr<NewCreditCardBubbleView> NewCreditCardBubbleView::Create(
    NewCreditCardBubbleController* controller) {
  NewCreditCardBubbleCocoa* bubble = new NewCreditCardBubbleCocoa(controller);
  return bubble->weak_ptr_factory_.GetWeakPtr();
}

NewCreditCardBubbleCocoa::NewCreditCardBubbleCocoa(
    NewCreditCardBubbleController* controller)
    : bubbleController_(NULL),
      controller_(controller),
      weak_ptr_factory_(this) {
}

void NewCreditCardBubbleCocoa::CreateCocoaController(NSWindow* parent) {
  DCHECK(!bubbleController_);
  bubbleController_ = [[NewCreditCardBubbleControllerCocoa alloc]
      initWithParentWindow:parent
                controller:controller_
                    bridge:this];
}

InfoBubbleWindow* NewCreditCardBubbleCocoa::GetInfoBubbleWindow() {
  return base::mac::ObjCCastStrict<InfoBubbleWindow>(
      [bubbleController_ window]);
}

}  // namespace autofill

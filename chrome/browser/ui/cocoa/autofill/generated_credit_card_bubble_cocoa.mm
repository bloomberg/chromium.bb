// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/generated_credit_card_bubble_cocoa.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_view.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/native_theme/native_theme.h"

using autofill::GeneratedCreditCardBubbleCocoa;

namespace {

const CGFloat kTitlePadding = 20.0;
const CGFloat kInset = 20.0;

}  // namespace

// The Cocoa side of the bubble controller.
@interface GeneratedCreditCardBubbleControllerCocoa :
    BaseBubbleController<NSTextViewDelegate> {

  // Bridge to C++ side.
  scoped_ptr<GeneratedCreditCardBubbleCocoa> bridge_;
}

- (id)initWithParentWindow:(NSWindow*)parentWindow
                    bridge:(GeneratedCreditCardBubbleCocoa*)bridge;

// Show the bubble at the given |anchor| point. Coordinates are in screen space.
- (void)showAtAnchor:(NSPoint)anchor;

// Return true if the bubble is in the process of hiding.
- (BOOL)isHiding;

// Build the window contents and lay them out.
- (void)performLayoutWithController:
      (autofill::GeneratedCreditCardBubbleController*)controller;

@end


@implementation GeneratedCreditCardBubbleControllerCocoa

- (id)initWithParentWindow:(NSWindow*)parentWindow
                    bridge:(GeneratedCreditCardBubbleCocoa*)bridge {
  DCHECK(bridge);
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    [window setCanBecomeKeyWindow:NO];
    bridge_.reset(bridge);

    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [[self bubble] setAlignment:info_bubble::kAlignArrowToAnchor];
    [[self bubble] setArrowLocation:info_bubble::kTopRight];
    [[self bubble] setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground))];
    [self performLayoutWithController:bridge->controller().get()];
  }
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  bridge_->OnBubbleClosing();
  [super windowWillClose:notification];
}

// Called when embedded links are clicked.
- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  bridge_->OnLinkClicked();
  return YES;
}

- (void)showAtAnchor:(NSPoint)anchorPoint {
  [self setAnchorPoint:anchorPoint];
  [self showWindow:nil];
}

- (BOOL)isHiding {
  InfoBubbleWindow* window =
      base::mac::ObjCCastStrict<InfoBubbleWindow>([self window]);
  return [window isClosing];
}


- (void)performLayoutWithController:
    (autofill::GeneratedCreditCardBubbleController*)controller {
  CGFloat bubbleWidth = autofill::GeneratedCreditCardBubbleView::kContentsWidth;

  // Build the bubble title.
  NSFont* titleFont = [NSFont systemFontOfSize:15.0];
  NSString* title = base::SysUTF16ToNSString(controller->TitleText());
  base::scoped_nsobject<NSTextField> titleView(
    [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [titleView setDrawsBackground:NO];
  [titleView setBezeled:NO];
  [titleView setEditable:NO];
  [titleView setSelectable:NO];
  [titleView setStringValue:title];
  [titleView setFont:titleFont];
  [titleView sizeToFit];

  bubbleWidth = std::max(bubbleWidth, NSWidth([titleView frame]) + 2 * kInset);

  // Build the contents view.
  base::scoped_nsobject<HyperlinkTextView> contentsView(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);

  [contentsView setEditable:NO];
  [contentsView setDelegate:self];

  NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
  NSFont* boldFont = [NSFont boldSystemFontOfSize:[NSFont systemFontSize]];
  [contentsView setMessage:base::SysUTF16ToNSString(controller->ContentsText())
                  withFont:font
              messageColor:[NSColor blackColor]];

  const std::vector<autofill::TextRange>& text_ranges =
      controller->ContentsTextRanges();
  for (size_t i = 0; i < text_ranges.size(); ++i) {
    NSRange range = text_ranges[i].range.ToNSRange();
    if (text_ranges[i].is_link) {
      [contentsView addLinkRange:range
                        withName:@(i)
                       linkColor:[NSColor blueColor]];
    } else {
       [[contentsView textStorage]
           addAttributes:@{ NSFontAttributeName : boldFont }
                   range:range];
    }
  }

  [contentsView setVerticallyResizable:YES];
  [contentsView setFrameSize:NSMakeSize(bubbleWidth - 2 * kInset, CGFLOAT_MAX)];
  [contentsView sizeToFit];

  // Sizes are computed, now lay out the individual parts.
  [contentsView setFrameOrigin:NSMakePoint(kInset, kInset)];
  [titleView setFrameOrigin:
      NSMakePoint(kInset, NSMaxY([contentsView frame]) + kTitlePadding)];
  [[[self window] contentView] setSubviews:@[ contentsView, titleView ]];

  // Update window frame.
  NSRect windowFrame = [[self window] frame];
  windowFrame.size =
      NSMakeSize(bubbleWidth, NSMaxY([titleView frame]) + kInset);
  [[self window] setFrame:windowFrame display:YES];
}

@end


namespace autofill {

// static
base::WeakPtr<GeneratedCreditCardBubbleView>
    GeneratedCreditCardBubbleView::Create(
        const base::WeakPtr<GeneratedCreditCardBubbleController>& controller) {
  return (new GeneratedCreditCardBubbleCocoa(controller))->weak_ptr_factory_.
      GetWeakPtr();
}

GeneratedCreditCardBubbleCocoa::GeneratedCreditCardBubbleCocoa(
    const base::WeakPtr<GeneratedCreditCardBubbleController>& controller)
    : bubbleController_(nil),
      controller_(controller),
      weak_ptr_factory_(this) {
}

GeneratedCreditCardBubbleCocoa::~GeneratedCreditCardBubbleCocoa() {}

void GeneratedCreditCardBubbleCocoa::Show() {
  DCHECK(controller_.get());
  NSView* browser_view =
      controller_->web_contents()->GetNativeView();
  NSWindow* parent_window = [browser_view window];
  LocationBarViewMac* location_bar =
      [[parent_window windowController] locationBarBridge];

  // |location_bar| can be NULL during initialization stages.
  if (!location_bar) {
    // Technically, Show() should never be called during initialization.
    NOTREACHED();

    delete this;
    return;
  }

  if (!bubbleController_) {
    bubbleController_ = [[GeneratedCreditCardBubbleControllerCocoa alloc]
        initWithParentWindow:parent_window
                      bridge:this];
  }

  // |bubbleController_| is supposed to take ownership of |this|. If it can't be
  // constructed, clean up and abort showing.
  if (!bubbleController_) {
    delete this;
    return;
  }

  NSPoint anchor = location_bar->GetGeneratedCreditCardBubblePoint();
  [bubbleController_ showAtAnchor:[parent_window convertBaseToScreen:anchor]];
}

void GeneratedCreditCardBubbleCocoa::Hide() {
  [bubbleController_ close];
}

bool GeneratedCreditCardBubbleCocoa::IsHiding() const {
  return [bubbleController_ isHiding];
}

void GeneratedCreditCardBubbleCocoa::OnBubbleClosing() {
  bubbleController_ = nil;
}

void GeneratedCreditCardBubbleCocoa::OnLinkClicked() {
  if (controller_)
    controller_->OnLinkClicked();
}

}  // autofill

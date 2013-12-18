// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/validation_message_bubble_controller.h"
#include "chrome/browser/ui/validation_message_bubble.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "grit/theme_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/base_view.h"
#import "ui/base/cocoa/flipped_view.h"
#include "ui/base/resource/resource_bundle.h"

const CGFloat kWindowInitialWidth = 200;
const CGFloat kWindowInitialHeight = 100;
const CGFloat kWindowMinWidth = 64;
const CGFloat kWindowMaxWidth = 256;
const CGFloat kWindowPadding = 8;
const CGFloat kIconTextMargin = 4;
const CGFloat kTextVerticalMargin = 4;

@implementation ValidationMessageBubbleController

- (id)init:(NSWindow*)parentWindow
anchoredAt:(NSPoint)anchorPoint
  mainText:(const base::string16&)mainText
   subText:(const base::string16&)subText {

  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:
              NSMakeRect(0, 0, kWindowInitialWidth, kWindowInitialHeight)
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  if ((self = [super initWithWindow:window.get()
                       parentWindow:parentWindow
                         anchoredAt:anchorPoint])) {
    [[self bubble] setArrowLocation:info_bubble::kTopLeft];
    self.shouldOpenAsKeyWindow = NO;

    NSView* contentView = [ValidationMessageBubbleController
        constructContentView:mainText subText:subText];
    [[window contentView] addSubview:contentView];
    NSRect contentFrame = [contentView frame];
    NSRect windowFrame = [window frame];
    windowFrame.size.width = NSWidth(contentFrame) + kWindowPadding * 2;
    windowFrame.size.height = NSHeight(contentFrame) + kWindowPadding * 2
        + info_bubble::kBubbleArrowHeight;
    [window setFrame:windowFrame display:nil];

    [self showWindow:nil];
  }
  return self;
}

+ (NSView*)constructContentView:(const base::string16&)mainText
                        subText:(const base::string16&)subText {
  NSRect contentFrame = NSMakeRect(kWindowPadding, kWindowPadding, 0, 0);
  FlippedView* contentView = [[FlippedView alloc] initWithFrame:contentFrame];

  NSImage* image = ResourceBundle::GetSharedInstance()
      .GetNativeImageNamed(IDR_INPUT_ALERT).ToNSImage();
  base::scoped_nsobject<NSImageView> imageView([[NSImageView alloc]
      initWithFrame:NSMakeRect(0, 0, image.size.width, image.size.height)]);
  [imageView setImageFrameStyle:NSImageFrameNone];
  [imageView setImage:image];
  [contentView addSubview:imageView];
  contentFrame.size.height = image.size.height;

  const CGFloat textX = NSWidth([imageView frame]) + kIconTextMargin;
  NSRect textFrame = NSMakeRect(textX, 0, NSWidth(contentFrame) - textX, 0);
  base::scoped_nsobject<NSTextField> text(
      [[NSTextField alloc] initWithFrame:textFrame]);
  [text setStringValue:base::SysUTF16ToNSString(mainText)];
  [text setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [text setEditable:NO];
  [text setBezeled:NO];
  const CGFloat minTextWidth = kWindowMinWidth - kWindowPadding * 2 - textX;
  const CGFloat maxTextWidth = kWindowMaxWidth - kWindowPadding * 2 - textX;
  [text sizeToFit];
  [contentView addSubview:text];
  textFrame = [text frame];
  if (NSWidth(textFrame) < minTextWidth) {
    textFrame.size.width = minTextWidth;
    [text setFrame:textFrame];
  } else if (NSWidth(textFrame) > maxTextWidth) {
    textFrame.size.width = maxTextWidth;
    textFrame.size.height = 0;
    [text setFrame:textFrame];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:text];
    textFrame = [text frame];
  }
  contentFrame.size.width = NSMaxX(textFrame);
  contentFrame.size.height =
      std::max(NSHeight(contentFrame), NSHeight(textFrame));

  if (!subText.empty()) {
    NSRect subTextFrame = NSMakeRect(
        textX, NSMaxY(textFrame) + kTextVerticalMargin,
        NSWidth(textFrame), 0);
    base::scoped_nsobject<NSTextField> text2(
        [[NSTextField alloc] initWithFrame:subTextFrame]);
    [text2 setStringValue:base::SysUTF16ToNSString(subText)];
    [text2 setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [text2 setEditable:NO];
    [text2 setBezeled:NO];
    [text2 sizeToFit];
    subTextFrame = [text2 frame];
    if (NSWidth(subTextFrame) > maxTextWidth) {
      subTextFrame.size.width = maxTextWidth;
      [text2 setFrame:subTextFrame];
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:text2];
      subTextFrame = [text2 frame];
    }
    [contentView addSubview:text2];
    contentFrame.size.width =
        std::max(NSWidth(contentFrame), NSMaxX(subTextFrame));
    contentFrame.size.height =
        std::max(NSHeight(contentFrame), NSMaxY(subTextFrame));
  }

  [contentView setFrame:contentFrame];
  return contentView;
}


@end  // implementation ValidationMessageBubbleCocoa

// ----------------------------------------------------------------

namespace {

// Converts |anchor_in_root_view| in rwhv coordinates to cocoa screen
// coordinates, and returns an NSPoint at the center of the bottom side of the
// converted rectangle.
NSPoint GetAnchorPoint(content::RenderWidgetHost* widget_host,
                       const gfx::Rect& anchor_in_root_view) {
  BaseView* view = base::mac::ObjCCastStrict<BaseView>(
      widget_host->GetView()->GetNativeView());
  NSRect cocoaRect = [view flipRectToNSRect:anchor_in_root_view];
  NSRect windowRect = [view convertRect:cocoaRect toView:nil];
  NSPoint point = NSMakePoint(NSMidX(windowRect), NSMinY(windowRect));
  return [[view window] convertBaseToScreen:point];
}

class ValidationMessageBubbleCocoa : public chrome::ValidationMessageBubble {
 public:
  ValidationMessageBubbleCocoa(content::RenderWidgetHost* widget_host,
                               const gfx::Rect& anchor_in_root_view,
                               const base::string16& main_text,
                               const base::string16& sub_text) {
    controller_.reset([[[ValidationMessageBubbleController alloc]
                        init:[widget_host->GetView()->GetNativeView() window]
                  anchoredAt:GetAnchorPoint(widget_host, anchor_in_root_view)
                    mainText:main_text
                     subText:sub_text] retain]);
  }

  virtual ~ValidationMessageBubbleCocoa() {
    [controller_.get() close];
  }

  virtual void SetPositionRelativeToAnchor(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_root_view) OVERRIDE {
    [controller_.get()
        setAnchorPoint:GetAnchorPoint(widget_host, anchor_in_root_view)];
  }

 private:
  base::scoped_nsobject<ValidationMessageBubbleController> controller_;
};

}

// ----------------------------------------------------------------

namespace chrome {

scoped_ptr<ValidationMessageBubble> ValidationMessageBubble::CreateAndShow(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_root_view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  return scoped_ptr<ValidationMessageBubble>(new ValidationMessageBubbleCocoa(
      widget_host, anchor_in_root_view, main_text, sub_text)).Pass();
}

}

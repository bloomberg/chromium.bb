// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/validation_message_bubble.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/validation_message_bubble_controller.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

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
  mainText:(const string16&)mainText
   subText:(const string16&)subText {

  scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:
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

+ (NSView*)constructContentView:(const string16&)mainText
                        subText:(const string16&)subText {
  NSRect contentFrame = NSMakeRect(kWindowPadding, kWindowPadding, 0, 0);
  FlippedView* contentView = [[FlippedView alloc] initWithFrame:contentFrame];

  NSImage* image = ResourceBundle::GetSharedInstance()
      .GetNativeImageNamed(IDR_INPUT_ALERT).ToNSImage();
  scoped_nsobject<NSImageView> imageView([[NSImageView alloc]
      initWithFrame:NSMakeRect(0, 0, image.size.width, image.size.height)]);
  [imageView setImageFrameStyle:NSImageFrameNone];
  [imageView setImage:image];
  [contentView addSubview:imageView];
  contentFrame.size.height = image.size.height;

  const CGFloat textX = NSWidth([imageView frame]) + kIconTextMargin;
  NSRect textFrame = NSMakeRect(textX, 0, NSWidth(contentFrame) - textX, 0);
  scoped_nsobject<NSTextField> text(
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
    scoped_nsobject<NSTextField> text2(
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

class ValidationMessageBubbleCocoa : public chrome::ValidationMessageBubble {
 public:
  ValidationMessageBubbleCocoa(content::RenderWidgetHost* widget_host,
                               const gfx::Rect& anchor_in_screen,
                               const string16& main_text,
                               const string16& sub_text) {
    NSWindow* parent_window = [widget_host->GetView()->GetNativeView() window];
    NSPoint anchor_point = NSMakePoint(
        anchor_in_screen.x() + anchor_in_screen.width() / 2,
        NSHeight([[parent_window screen] frame])
            - (anchor_in_screen.y() + anchor_in_screen.height()));
    controller_.reset([[[ValidationMessageBubbleController alloc]
                        init:parent_window
                  anchoredAt:anchor_point
                    mainText:main_text
                     subText:sub_text] retain]);
  }

  virtual ~ValidationMessageBubbleCocoa() {
    [controller_.get() close];
  }

 private:
  scoped_nsobject<ValidationMessageBubbleController> controller_;
};

}

// ----------------------------------------------------------------

namespace chrome {

scoped_ptr<ValidationMessageBubble> ValidationMessageBubble::CreateAndShow(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_screen,
    const string16& main_text,
    const string16& sub_text) {
  return scoped_ptr<ValidationMessageBubble>(new ValidationMessageBubbleCocoa(
      widget_host, anchor_in_screen, main_text, sub_text)).Pass();
}

}

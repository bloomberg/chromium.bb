// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screen_capture_notification_ui_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"

const CGFloat kMinimumWidth = 460;
const CGFloat kMaximumWidth = 1000;
const CGFloat kHorizontalMargin = 10;
const CGFloat kPadding = 5;
const CGFloat kPaddingLeft = 10;
const CGFloat kWindowCornerRadius = 2;

@interface ScreenCaptureNotificationController()
- (void)hide;
- (void)populateWithText:(const string16&)text;
@end

@interface ScreenCaptureNotificationView : NSView
@end

@interface WindowGripView : NSImageView
- (WindowGripView*)init;
@end


ScreenCaptureNotificationUICocoa::ScreenCaptureNotificationUICocoa(
    const string16& text)
    : text_(text) {
}

ScreenCaptureNotificationUICocoa::~ScreenCaptureNotificationUICocoa() {}

void ScreenCaptureNotificationUICocoa::OnStarted(
    const base::Closure& stop_callback) {
  DCHECK(!stop_callback.is_null());
  DCHECK(!windowController_);

  windowController_.reset([[ScreenCaptureNotificationController alloc]
      initWithCallback:stop_callback
                  text:text_]);
  [windowController_ showWindow:nil];
}

scoped_ptr<ScreenCaptureNotificationUI> ScreenCaptureNotificationUI::Create(
    const string16& text) {
  return scoped_ptr<ScreenCaptureNotificationUI>(
      new ScreenCaptureNotificationUICocoa(text));
}

@implementation ScreenCaptureNotificationController
- (id)initWithCallback:(const base::Closure&)stop_callback
                  text:(const string16&)text {
  base::scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [window setReleasedWhenClosed:NO];
  [window setBackgroundColor:[NSColor clearColor]];
  [window setOpaque:NO];
  [window setHasShadow:YES];
  [window setLevel:NSStatusWindowLevel];
  [window setMovableByWindowBackground:YES];
  [window setDelegate:self];

  self = [super initWithWindow:window];
  if (self) {
    stop_callback_ = stop_callback;
    [self populateWithText:text];

    // Center the window at the bottom of the screen, above the dock (if
    // present).
    NSRect desktopRect = [[NSScreen mainScreen] visibleFrame];
    NSRect contentRect = [[window contentView] frame];
    NSRect windowRect =
        NSMakeRect((NSWidth(desktopRect) - NSWidth(contentRect)) / 2,
                   NSMinY(desktopRect),
                   NSWidth(contentRect),
                   NSHeight(contentRect));
    [window setFrame:windowRect display:YES];
  }
  return self;
}

- (void)stopSharing:(id)sender {
  if (!stop_callback_.is_null()) {
    base::Closure callback = stop_callback_;
    stop_callback_.Reset();
    callback.Run();  // Deletes |self|.
  }
}

- (void)hide {
  stop_callback_.Reset();
  [self close];
}

- (void)populateWithText:(const string16&)text {
  base::scoped_nsobject<ScreenCaptureNotificationView> content(
      [[ScreenCaptureNotificationView alloc]
          initWithFrame:ui::kWindowSizeDeterminedLater]);
  [[self window] setContentView:content];

  // Create button.
  stopButton_.reset([[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
  [stopButton_ setTitle:l10n_util::GetNSString(
                  IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP)];
  [stopButton_ setTarget:self];
  [stopButton_ setAction:@selector(stopSharing:)];
  [stopButton_ sizeToFit];
  [content addSubview:stopButton_];

  CGFloat buttonWidth = NSWidth([stopButton_ frame]);
  CGFloat totalHeight = kPadding + NSHeight([stopButton_ frame]) + kPadding;

  // Create grip icon.
  base::scoped_nsobject<WindowGripView> gripView([[WindowGripView alloc] init]);
  [content addSubview:gripView];
  CGFloat gripWidth = NSWidth([gripView frame]);
  CGFloat gripHeight = NSHeight([gripView frame]);
  [gripView
      setFrameOrigin:NSMakePoint(kPaddingLeft, (totalHeight - gripHeight) / 2)];

  // Create text label.
  int maximumWidth =
      std::min(kMaximumWidth, NSWidth([[NSScreen mainScreen] visibleFrame]));
  int maxLabelWidth = maximumWidth - kPaddingLeft - kPadding -
                      kHorizontalMargin * 2 - gripWidth - buttonWidth;
  gfx::Font font(ui::ResourceBundle::GetSharedInstance()
                     .GetFont(ui::ResourceBundle::BaseFont)
                     .GetNativeFont());
  string16 elidedText =
      ElideText(text, font, maxLabelWidth, gfx::ELIDE_IN_MIDDLE);
  NSString* statusText = base::SysUTF16ToNSString(elidedText);
  base::scoped_nsobject<NSTextField> statusTextField(
      [[NSTextField alloc] initWithFrame:ui::kWindowSizeDeterminedLater]);
  [statusTextField setEditable:NO];
  [statusTextField setSelectable:NO];
  [statusTextField setDrawsBackground:NO];
  [statusTextField setBezeled:NO];
  [statusTextField setStringValue:statusText];
  [statusTextField setFont:font.GetNativeFont()];
  [statusTextField sizeToFit];
  [statusTextField setFrameOrigin:NSMakePoint(
                       kPaddingLeft + kHorizontalMargin + gripWidth,
                       (totalHeight - NSHeight([statusTextField frame])) / 2)];
  [content addSubview:statusTextField];

  // Resize content view to fit controls.
  CGFloat minimumLableWidth = kMinimumWidth - kPaddingLeft - kPadding -
                              kHorizontalMargin * 2 - gripWidth - buttonWidth;
  CGFloat lableWidth =
      std::max(NSWidth([statusTextField frame]), minimumLableWidth);
  CGFloat totalWidth = kPaddingLeft + kPadding + kHorizontalMargin * 2 +
                       gripWidth + lableWidth + buttonWidth;
  [content setFrame:NSMakeRect(0, 0, totalWidth, totalHeight)];

  // Move the button to the right place.
  NSPoint buttonOrigin =
      NSMakePoint(totalWidth - kPadding - buttonWidth, kPadding);
  [stopButton_ setFrameOrigin:buttonOrigin];

  if (base::i18n::IsRTL()) {
    [stopButton_
        setFrameOrigin:NSMakePoint(totalWidth - NSMaxX([stopButton_ frame]),
                                   NSMinY([stopButton_ frame]))];
    [statusTextField
        setFrameOrigin:NSMakePoint(totalWidth - NSMaxX([statusTextField frame]),
                                   NSMinY([statusTextField frame]))];
    [gripView setFrameOrigin:NSMakePoint(totalWidth - NSMaxX([gripView frame]),
                                         NSMinY([gripView frame]))];
  }
}

- (void)windowWillClose:(NSNotification*)notification {
  [self stopSharing:nil];
}

@end

@implementation ScreenCaptureNotificationView

- (void)drawRect:(NSRect)dirtyRect {
  [gfx::SkColorToSRGBNSColor(ui::NativeTheme::instance()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground)) set];
  [[NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                   xRadius:kWindowCornerRadius
                                   yRadius:kWindowCornerRadius] fill];
}

@end

@implementation WindowGripView
- (WindowGripView*)init {
  gfx::Image gripImage =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_SCREEN_CAPTURE_NOTIFICATION_GRIP,
          ui::ResourceBundle::RTL_DISABLED);
  self = [super
      initWithFrame:NSMakeRect(0, 0, gripImage.Width(), gripImage.Height())];
  [self setImage:gripImage.ToNSImage()];
  return self;
}

- (BOOL)mouseDownCanMoveWindow {
  return YES;
}
@end

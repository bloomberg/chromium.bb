// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/theme_install_bubble_view.h"

#include "base/memory/scoped_nsobject.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// The alpha of the bubble.
static const float kBubbleAlpha = 0.75;

// The roundedness of the edges of our bubble.
static const int kBubbleCornerRadius = 4;

// Padding around text in popup box.
static const int kTextHorizPadding = 90;
static const int kTextVertPadding = 45;

// Point size of the text in the box.
static const int kLoadingTextSize = 24;

}

// static
ThemeInstallBubbleView* ThemeInstallBubbleView::view_ = NULL;

// The Cocoa view to draw a gray rounded rect with "Loading..." in it.
@interface ThemeInstallBubbleViewCocoa : NSView {
 @private
  scoped_nsobject<NSAttributedString> message_;

  NSRect grayRect_;
  NSRect textRect_;
}

- (id)init;

// The size of the gray rect that will be drawn.
- (NSSize)preferredSize;
// Forces size calculations of where everything will be drawn.
- (void)layout;

@end

ThemeInstallBubbleView::ThemeInstallBubbleView(NSWindow* window)
    : cocoa_view_([[ThemeInstallBubbleViewCocoa alloc] init]),
      num_loads_extant_(1) {
  DCHECK(window);

  NSView* parent_view = [window contentView];
  NSRect parent_bounds = [parent_view bounds];
  if (parent_bounds.size.height < [cocoa_view_ preferredSize].height)
    Close();

  // Close when theme has been installed.
  registrar_.Add(
      this,
      NotificationType::BROWSER_THEME_CHANGED,
      NotificationService::AllSources());

  // Close when we are installing an extension, not a theme.
  registrar_.Add(
      this,
      NotificationType::NO_THEME_DETECTED,
      NotificationService::AllSources());
  registrar_.Add(
      this,
      NotificationType::EXTENSION_INSTALLED,
      NotificationService::AllSources());
  registrar_.Add(
      this,
      NotificationType::EXTENSION_INSTALL_ERROR,
      NotificationService::AllSources());

  // Don't let the bubble overlap the confirm dialog.
  registrar_.Add(
      this,
      NotificationType::EXTENSION_WILL_SHOW_CONFIRM_DIALOG,
      NotificationService::AllSources());

  // Add the view.
  [cocoa_view_ setFrame:parent_bounds];
  [cocoa_view_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [parent_view addSubview:cocoa_view_
               positioned:NSWindowAbove
               relativeTo:nil];
  [cocoa_view_ layout];
}

ThemeInstallBubbleView::~ThemeInstallBubbleView() {
  // Need to delete self; the real work happens in Close().
}

void ThemeInstallBubbleView::Close() {
  --num_loads_extant_;
  if (num_loads_extant_ < 1) {
    registrar_.RemoveAll();
    if (cocoa_view_ && [cocoa_view_ superview]) {
      [cocoa_view_ removeFromSuperview];
      [cocoa_view_ release];
    }
    view_ = NULL;
    delete this;
    // this is deleted; nothing more!
  }
}

void ThemeInstallBubbleView::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  Close();
}

// static
void ThemeInstallBubbleView::Show(NSWindow* window) {
  if (view_)
    ++view_->num_loads_extant_;
  else
    view_ = new ThemeInstallBubbleView(window);
}

@implementation ThemeInstallBubbleViewCocoa

- (id)init {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    NSString* loadingString =
        l10n_util::GetNSStringWithFixup(IDS_THEME_LOADING_TITLE);
    NSFont* loadingFont = [NSFont systemFontOfSize:kLoadingTextSize];
    NSColor* textColor = [NSColor whiteColor];
    NSDictionary* loadingAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
                                  loadingFont, NSFontAttributeName,
                                  textColor, NSForegroundColorAttributeName,
                                  nil];
    message_.reset([[NSAttributedString alloc] initWithString:loadingString
                                                   attributes:loadingAttrs]);

    // TODO(avi): find a white-on-black spinner
  }
  return self;
}

- (NSSize)preferredSize {
  NSSize size = [message_.get() size];
  size.width += kTextHorizPadding;
  size.height += kTextVertPadding;
  return size;
}

// Update the layout to keep the view centered when the window is resized.
- (void)resizeWithOldSuperviewSize:(NSSize)oldBoundsSize {
  [super resizeWithOldSuperviewSize:oldBoundsSize];
  [self layout];
}

- (void)layout {
  NSRect bounds = [self bounds];

  grayRect_.size = [self preferredSize];
  grayRect_.origin.x = (bounds.size.width - grayRect_.size.width) / 2;
  grayRect_.origin.y = bounds.size.height / 2;

  textRect_.size = [message_.get() size];
  textRect_.origin.x = (bounds.size.width - [message_.get() size].width) / 2;
  textRect_.origin.y = (bounds.size.height + kTextVertPadding) / 2;
}

- (void)drawRect:(NSRect)dirtyRect {
  [[NSColor clearColor] set];
  NSRectFillUsingOperation([self bounds], NSCompositeSourceOver);

  [[[NSColor blackColor] colorWithAlphaComponent:kBubbleAlpha] set];
  [[NSBezierPath bezierPathWithRoundedRect:grayRect_
                                   xRadius:kBubbleCornerRadius
                                   yRadius:kBubbleCornerRadius] fill];

  [message_.get() drawInRect:textRect_];
}

@end

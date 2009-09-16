// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/status_bubble_mac.h"

#include "app/gfx/text_elider.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/bubble_view.h"
#include "googleurl/src/gurl.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

namespace {

const int kWindowHeight = 18;
// The width of the bubble in relation to the width of the parent window.
const float kWindowWidthPercent = 1.0f/3.0f;

// How close the mouse can get to the infobubble before it starts sliding
// off-screen.
const int kMousePadding = 20;

const int kTextPadding = 3;

// How long each fade should last for.
const int kShowFadeDuration = 0.120f;
const int kHideFadeDuration = 0.200f;

}

// TODO(avi):
// - do display delay

StatusBubbleMac::StatusBubbleMac(NSWindow* parent, id delegate)
    : parent_(parent),
      delegate_(delegate),
      window_(nil),
      status_text_(nil),
      url_text_(nil) {
}

StatusBubbleMac::~StatusBubbleMac() {
  Hide();
}

void StatusBubbleMac::SetStatus(const std::wstring& status) {
  Create();

  NSString* status_ns = base::SysWideToNSString(status);

  SetStatus(status_ns, false);
}

void StatusBubbleMac::SetURL(const GURL& url, const std::wstring& languages) {
  Create();

  NSRect frame = [window_ frame];
  int text_width = static_cast<int>(frame.size.width -
                                    kBubbleViewTextPositionX -
                                    kTextPadding);
  NSFont* font = [[window_ contentView] font];
  gfx::Font font_chr =
      gfx::Font::CreateFont(base::SysNSStringToWide([font fontName]),
                            [font pointSize]);

  std::wstring status = gfx::ElideUrl(url, font_chr, text_width, languages);
  NSString* status_ns = base::SysWideToNSString(status);

  SetStatus(status_ns, true);
}

void StatusBubbleMac::SetStatus(NSString* status, bool is_url) {
  NSString** main;
  NSString** backup;

  if (is_url) {
    main = &url_text_;
    backup = &status_text_;
  } else {
    main = &status_text_;
    backup = &url_text_;
  }

  if ([status isEqualToString:*main])
    return;

  [*main release];
  *main = [status retain];
  if ([*main length] > 0) {
    [[window_ contentView] setContent:*main];
  } else if ([*backup length] > 0) {
    [[window_ contentView] setContent:*backup];
  } else {
    Hide();
  }

  FadeIn();
}

void StatusBubbleMac::Hide() {
  FadeOut();

  if (window_) {
    [parent_ removeChildWindow:window_];
    [window_ release];
    window_ = nil;
  }

  [status_text_ release];
  status_text_ = nil;
  [url_text_ release];
  url_text_ = nil;
}

void StatusBubbleMac::MouseMoved() {
  if (!window_)
    return;

  NSPoint cursor_location = [NSEvent mouseLocation];
  --cursor_location.y;  // docs say the y coord starts at 1 not 0; don't ask why

  // Get the normal position of the frame.
  NSRect window_frame = [window_ frame];
  window_frame.origin = [parent_ frame].origin;

  // Adjust the position to sit on top of download and extension shelves.
  // |delegate_| can be nil during unit tests.
  if ([delegate_ respondsToSelector:@selector(verticalOffsetForStatusBubble)])
    window_frame.origin.y += [delegate_ verticalOffsetForStatusBubble];

  // Get the cursor position relative to the popup.
  cursor_location.x -= NSMaxX(window_frame);
  cursor_location.y -= NSMaxY(window_frame);

  // If the mouse is in a position where we think it would move the
  // status bubble, figure out where and how the bubble should be moved.
  if (cursor_location.y < kMousePadding &&
      cursor_location.x < kMousePadding) {
    int offset = kMousePadding - cursor_location.y;

    // Make the movement non-linear.
    offset = offset * offset / kMousePadding;

    // When the mouse is entering from the right, we want the offset to be
    // scaled by how horizontally far away the cursor is from the bubble.
    if (cursor_location.x > 0) {
      offset = offset * ((kMousePadding - cursor_location.x) / kMousePadding);
    }

    // Cap the offset and change the visual presentation of the bubble
    // depending on where it ends up (so that rounded corners square off
    // and mate to the edges of the tab content).
    if (offset >= NSHeight(window_frame)) {
      offset = NSHeight(window_frame);
      [[window_ contentView] setCornerFlags:
          kRoundedBottomLeftCorner | kRoundedBottomRightCorner];
    } else if (offset > 0) {
      [[window_ contentView] setCornerFlags:
          kRoundedTopRightCorner | kRoundedBottomLeftCorner |
          kRoundedBottomRightCorner];
    } else {
      [[window_ contentView] setCornerFlags:kRoundedTopRightCorner];
    }

    offset_ = offset;
    window_frame.origin.y -= offset;
  } else {
    offset_ = 0;
    [[window_ contentView] setCornerFlags:kRoundedTopRightCorner];
  }

  [window_ setFrame:window_frame display:YES];
}

void StatusBubbleMac::UpdateDownloadShelfVisibility(bool visible) {
}

void StatusBubbleMac::Create() {
  if (window_)
    return;

  NSRect rect = [parent_ frame];
  rect.size.height = kWindowHeight;
  rect.size.width = static_cast<int>(kWindowWidthPercent * rect.size.width);
  // TODO(avi):fix this for RTL
  window_ = [[NSWindow alloc] initWithContentRect:rect
                                        styleMask:NSBorderlessWindowMask
                                          backing:NSBackingStoreBuffered
                                            defer:YES];
  [window_ setMovableByWindowBackground:NO];
  [window_ setBackgroundColor:[NSColor clearColor]];
  [window_ setLevel:NSNormalWindowLevel];
  [window_ setOpaque:NO];
  [window_ setHasShadow:NO];

  // We do not need to worry about the bubble outliving |parent_| because our
  // teardown sequence in BWC guarantees that |parent_| outlives the status
  // bubble and that the StatusBubble is torn down completely prior to the
  // window going away.
  scoped_nsobject<BubbleView> view(
      [[BubbleView alloc] initWithFrame:NSZeroRect themeProvider:parent_]);
  [window_ setContentView:view];

  [parent_ addChildWindow:window_ ordered:NSWindowAbove];

  [window_ setAlphaValue:0.0f];

  offset_ = 0;
  [view setCornerFlags:kRoundedTopRightCorner];
  MouseMoved();
}

void StatusBubbleMac::FadeIn() {
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:kShowFadeDuration];
  [[window_ animator] setAlphaValue:1.0f];
  [NSAnimationContext endGrouping];
}

void StatusBubbleMac::FadeOut() {
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:kHideFadeDuration];
  [[window_ animator] setAlphaValue:0.0f];
  [NSAnimationContext endGrouping];
}


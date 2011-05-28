// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"


// The info-bubble point should look like it points to the bottom of the lock
// icon. Determined with Pixie.app.
const CGFloat kBubblePointYOffset = 2.0;

LocationIconDecoration::LocationIconDecoration(LocationBarViewMac* owner)
    : owner_(owner) {
}

LocationIconDecoration::~LocationIconDecoration() {
}

bool LocationIconDecoration::IsDraggable() {
  // Without a tab it will be impossible to get the information needed
  // to perform a drag.
  if (!owner_->GetTabContents())
    return false;

  // Do not drag if the user has been editing the location bar, or the
  // location bar is at the NTP.
  if (owner_->location_entry()->IsEditingOrEmpty())
    return false;

  return true;
}

NSPasteboard* LocationIconDecoration::GetDragPasteboard() {
  TabContents* tab = owner_->GetTabContents();
  DCHECK(tab);  // See |IsDraggable()|.

  NSString* url = base::SysUTF8ToNSString(tab->GetURL().spec());
  NSString* title = base::SysUTF16ToNSString(tab->GetTitle());

  NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array]
                                            owner:nil];
  [pboard setDataForURL:url title:title];
  return pboard;
}

// Draws string |title| within box |frame|, positioning it at the origin.
// Truncates text with fading if it is too long to fit horizontally.
// Based on code from GradientButtonCell but simplified where possible.
void DrawTruncatedTitle(NSAttributedString* title, NSRect frame) {
  NSSize size = [title size];
  if (floor(size.width) <= NSWidth(frame)) {
    [title drawAtPoint:frame.origin];
    return;
  }
  // Gradient is about twice our line height long.
  CGFloat gradientWidth = MIN(size.height * 2, NSWidth(frame) / 4);
  NSRect solidPart, gradientPart;
  NSDivideRect(frame, &gradientPart, &solidPart, gradientWidth, NSMaxXEdge);
  CGContextRef context = static_cast<CGContextRef>(
                            [[NSGraphicsContext currentContext] graphicsPort]);
  CGContextBeginTransparencyLayerWithRect(context, NSRectToCGRect(frame), 0);
  { // Draw text clipped to frame.
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    [NSBezierPath clipRect:frame];
    [title drawAtPoint:frame.origin];
  }
  NSColor* color = [NSColor blackColor];
  NSColor* alphaColor = [color colorWithAlphaComponent:0.0];
  NSGradient* mask = [[[NSGradient alloc] initWithStartingColor:color
      endingColor:alphaColor] autorelease];
  // Draw the gradient mask.
  CGContextSetBlendMode(context, kCGBlendModeDestinationIn);
  [mask drawFromPoint:NSMakePoint(NSMaxX(frame) - gradientWidth,
                                  NSMinY(frame))
              toPoint:NSMakePoint(NSMaxX(frame),
                                  NSMinY(frame))
              options:NSGradientDrawsBeforeStartingLocation];
  CGContextEndTransparencyLayer(context);
}


NSImage* LocationIconDecoration::GetDragImage() {
  // Get the icon. Use favIcon if possible.
  const SkBitmap& favicon = owner_->GetFavicon();
  NSImage *iconImage = (!favicon.isNull()) ?
      gfx::SkBitmapToNSImage(favicon) : GetImage();

  // Get the title.
  NSString* title = base::SysUTF16ToNSString(owner_->GetTitle());
  // If no title, just use icon.
  if (![title length])
    return iconImage;

  // Set the look of the title.
  NSDictionary* attrs =
      [NSDictionary dictionaryWithObjectsAndKeys:
          [NSFont systemFontOfSize:[NSFont smallSystemFontSize]],
          NSFontAttributeName,
          nil];
  NSAttributedString* richTitle = [[[NSAttributedString alloc]
      initWithString:title
          attributes:attrs] autorelease];

  // Setup sizes and locations for rendering.
  const CGFloat kIconMargin = 2.0; // Gap between icon and text.
  CGFloat textLeft = [iconImage size].width + kIconMargin;
  NSSize dragImageSize = [iconImage size];
  NSSize textSize = [richTitle size];
  CGFloat maxTextWidth = bookmarks::kDefaultBookmarkWidth - textLeft - 16.0;
  BOOL truncate = textSize.width > maxTextWidth;
  if (truncate)
    textSize.width = maxTextWidth;
  dragImageSize.width = textLeft + textSize.width;

  // Render the drag image.
  NSImage* dragImage =
      [[[NSImage alloc] initWithSize:dragImageSize] autorelease];
  [dragImage lockFocus];
  [iconImage drawAtPoint:NSMakePoint(0, 0)
                fromRect:NSZeroRect
               operation:NSCompositeSourceOver
                fraction:0.7];
  NSRect targetTextRect = NSMakeRect(textLeft, 0,
                                     textSize.width, dragImageSize.height);
  DrawTruncatedTitle(richTitle, targetTextRect);
  [dragImage unlockFocus];

  return dragImage;
}

NSRect LocationIconDecoration::GetDragImageFrame(NSRect frame) {
  return GetDrawRectInFrame(frame);
}

NSPoint LocationIconDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame),
                     NSMaxY(draw_frame) - kBubblePointYOffset);
}

bool LocationIconDecoration::AcceptsMousePress() {
  return true;
}

bool LocationIconDecoration::OnMousePressed(NSRect frame) {
  // Do not show page info if the user has been editing the location
  // bar, or the location bar is at the NTP.
  if (owner_->location_entry()->IsEditingOrEmpty())
    return true;

  TabContents* tab = owner_->GetTabContents();
  NavigationEntry* nav_entry = tab->controller().GetActiveEntry();
  if (!nav_entry) {
    NOTREACHED();
    return true;
  }
  tab->ShowPageInfo(nav_entry->url(), nav_entry->ssl(), true);
  return true;
}

NSString* LocationIconDecoration::GetToolTip() {
  if (owner_->location_entry()->IsEditingOrEmpty())
    return nil;
  else
    return l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_LOCATION_ICON);
}

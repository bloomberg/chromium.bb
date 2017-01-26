// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/drag_util.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

// The info-bubble point should look like it points to the bottom of the lock
// icon. Determined with Pixie.app.
const CGFloat kBubblePointYOffset = 2.0;

// Insets for the background frame.
const CGFloat kBackgroundFrameXInset = 1.0;
const CGFloat kBackgroundFrameYInset = 2.0;

LocationIconDecoration::LocationIconDecoration(LocationBarViewMac* owner)
    : drag_frame_(NSZeroRect), owner_(owner) {
}

LocationIconDecoration::~LocationIconDecoration() {
}

bool LocationIconDecoration::IsDraggable() {
  // Without a tab it will be impossible to get the information needed
  // to perform a drag.
  if (!owner_->GetWebContents())
    return false;

  // Do not drag if the user has been editing the location bar, or the
  // location bar is at the NTP.
  return (!owner_->GetOmniboxView()->IsEditingOrEmpty());
}

NSPasteboard* LocationIconDecoration::GetDragPasteboard() {
  WebContents* tab = owner_->GetWebContents();
  DCHECK(tab);  // See |IsDraggable()|.

  NSString* url = base::SysUTF8ToNSString(tab->GetURL().spec());
  NSString* title = base::SysUTF16ToNSString(tab->GetTitle());

  NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  [pboard declareURLPasteboardWithAdditionalTypes:
        [NSArray arrayWithObject:NSFilesPromisePboardType]
                                            owner:nil];
  [pboard setDataForURL:url title:title];

  [pboard setPropertyList:[NSArray arrayWithObject:@"webloc"]
                  forType:NSFilesPromisePboardType];

  return pboard;
}

NSImage* LocationIconDecoration::GetDragImage() {
  content::WebContents* web_contents = owner_->GetWebContents();
  NSImage* favicon =
      favicon::ContentFaviconDriver::FromWebContents(web_contents)
          ->GetFavicon()
          .AsNSImage();
  NSImage* iconImage = favicon ? favicon : GetImage();

  NSImage* image =
      drag_util::DragImageForBookmark(iconImage,
                                      web_contents->GetTitle(),
                                      bookmarks::kDefaultBookmarkWidth);
  NSSize imageSize = [image size];
  drag_frame_ = NSMakeRect(0, 0, imageSize.width, imageSize.height);
  return image;
}

NSRect LocationIconDecoration::GetDragImageFrame(NSRect frame) {
  // If GetDragImage has never been called, drag_frame_ has not been calculated.
  if (NSIsEmptyRect(drag_frame_))
    GetDragImage();
  return drag_frame_;
}

NSPoint LocationIconDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame),
                     NSMaxY(draw_frame) - kBubblePointYOffset);
}

NSRect LocationIconDecoration::GetBackgroundFrame(NSRect frame) {
  return NSInsetRect(frame, kBackgroundFrameXInset, kBackgroundFrameYInset);
}

bool LocationIconDecoration::AcceptsMousePress() {
  return true;
}

bool LocationIconDecoration::HasHoverAndPressEffect() {
  // The search icon should not show a hover/pressed background.
  return !owner_->GetOmniboxView()->IsEditingOrEmpty();
}

bool LocationIconDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  // TODO(macourteau): this code (for displaying the page info bubble) should be
  // pulled out into LocationBarViewMac (or maybe even further), as other
  // decorations currently depend on this decoration only to show the page info
  // bubble.

  // Do not show page info if the user has been editing the location
  // bar, or the location bar is at the NTP.
  if (owner_->GetOmniboxView()->IsEditingOrEmpty())
    return true;

  WebContents* tab = owner_->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(tab);
  chrome::ShowWebsiteSettings(browser, tab);
  return true;
}

NSString* LocationIconDecoration::GetToolTip() {
  return owner_->GetOmniboxView()->IsEditingOrEmpty() ?
      nil : l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_LOCATION_ICON);
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "grit/generated_resources.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/l10n/l10n_util_mac.h"

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

NSImage* LocationIconDecoration::GetDragImage() {
  return GetImage();
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

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/cocoa/location_bar/location_icon_decoration.h"

#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"

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

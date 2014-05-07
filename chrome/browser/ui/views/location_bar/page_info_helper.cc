// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/page_info_helper.h"

#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

PageInfoHelper::PageInfoHelper(const views::View* owner,
                               LocationBarView* location_bar)
    : owner_(owner),
      location_bar_(location_bar) {
}

void PageInfoHelper::ProcessEvent(const ui::LocatedEvent& event) {
  if (!owner_->HitTestPoint(event.location()))
    return;

  WebContents* tab = location_bar_->GetWebContents();
  if (!tab)
    return;

  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  NavigationEntry* nav_entry = tab->GetController().GetVisibleEntry();
  // The visible entry can be NULL in the case of window.open("").
  if (!nav_entry)
    return;

  location_bar_->delegate()->ShowWebsiteSettings(
      tab, nav_entry->GetURL(), nav_entry->GetSSL());
}

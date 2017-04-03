// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/navigation_manager_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

web::NavigationItem* GetLastCommittedNonRedirectedItem(
    web::NavigationManager* nav_manager) {
  if (!nav_manager || !nav_manager->GetItemCount())
    return nullptr;
  int index = nav_manager->GetLastCommittedItemIndex();
  while (index >= 0) {
    web::NavigationItem* item = nav_manager->GetItemAtIndex(index);
    // Returns the first non-Redirect item found.
    if ((item->GetTransitionType() & ui::PAGE_TRANSITION_IS_REDIRECT_MASK) == 0)
      return item;
    --index;
  }

  return nullptr;
}

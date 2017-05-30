// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/navigation_manager_util.h"

#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

NavigationItem* GetItemWithUniqueID(NavigationManager* navigation_manager,
                                    int unique_id) {
  NavigationItem* transient_item = navigation_manager->GetTransientItem();
  if (transient_item && transient_item->GetUniqueID() == unique_id)
    return transient_item;

  NavigationItem* pending_item = navigation_manager->GetPendingItem();
  if (pending_item && pending_item->GetUniqueID() == unique_id)
    return pending_item;

  return GetCommittedItemWithUniqueID(navigation_manager, unique_id);
}

NavigationItem* GetCommittedItemWithUniqueID(
    NavigationManager* navigation_manager,
    int unique_id) {
  int index = GetCommittedItemIndexWithUniqueID(navigation_manager, unique_id);
  return index != -1 ? navigation_manager->GetItemAtIndex(index) : nullptr;
}

int GetCommittedItemIndexWithUniqueID(NavigationManager* navigation_manager,
                                      int unique_id) {
  for (int i = 0; i < navigation_manager->GetItemCount(); i++) {
    web::NavigationItem* item = navigation_manager->GetItemAtIndex(i);
    if (item->GetUniqueID() == unique_id) {
      return i;
    }
  }
  return -1;
}

}  // namespace web

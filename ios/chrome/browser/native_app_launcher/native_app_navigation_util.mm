// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/native_app_launcher/native_app_navigation_util.h"

#include "base/logging.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace native_app_launcher {

bool IsLinkNavigation(web::WebState* web_state) {
  web::NavigationManager* navigationManager = web_state->GetNavigationManager();
  DCHECK(navigationManager);
  int index = navigationManager->GetCurrentItemIndex();
  // Walks backward on the navigation items list looking for the first item
  // that is not the result of a redirect. Check if user arrived at that
  // via link click or a suggestion on the UI.
  while (index >= 0) {
    web::NavigationItem* item = navigationManager->GetItemAtIndex(index);
    DCHECK(item);
    ui::PageTransition currentTransition = item->GetTransitionType();
    // Checks non-redirect entries for transitions that are either links or
    // bookmarks.
    if ((currentTransition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK) == 0) {
      return PageTransitionCoreTypeIs(currentTransition,
                                      ui::PAGE_TRANSITION_LINK) ||
             PageTransitionCoreTypeIs(currentTransition,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    }
    --index;
  }
  return false;
}

}  // namespace native_app_launcher

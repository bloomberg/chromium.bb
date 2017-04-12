// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/native_app_launcher/native_app_navigation_util.h"

#import "ios/chrome/browser/web/navigation_manager_util.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace native_app_launcher {

bool IsLinkNavigation(web::WebState* web_state) {
  DCHECK(web_state);
  web::NavigationItem* item =
      GetLastCommittedNonRedirectedItem(web_state->GetNavigationManager());
  if (!item)
    return false;
  ui::PageTransition transition = item->GetTransitionType();
  return PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) ||
         PageTransitionCoreTypeIs(transition,
                                  ui::PAGE_TRANSITION_AUTO_BOOKMARK);
}

}  // namespace native_app_launcher

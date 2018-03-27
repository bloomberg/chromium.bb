// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_throttle.h"

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

// static
std::unique_ptr<content::NavigationThrottle>
AppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (arc::IsArcPlayStoreEnabledForProfile(Profile::FromBrowserContext(
          handle->GetWebContents()->GetBrowserContext())) &&
      !handle->GetWebContents()->GetBrowserContext()->IsOffTheRecord()) {
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(handle->GetWebContents());
    if (!prerender_contents)
      return std::make_unique<arc::ArcNavigationThrottle>(handle);
  }

  return nullptr;
}

}  // namespace chromeos

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

// static
std::unique_ptr<BackgroundTabNavigationThrottle>
BackgroundTabNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(features::kStaggeredBackgroundTabOpening))
    return nullptr;

  // Only consider main frames because this is to delay tabs.
  if (!navigation_handle->IsInMainFrame())
    return nullptr;

  // Never delay foreground tabs.
  if (navigation_handle->GetWebContents()->IsVisible())
    return nullptr;

  // Never delay the tab when there is opener, so the created window can talk
  // to the creator immediately.
  if (navigation_handle->GetWebContents()->HasOpener())
    return nullptr;

  // Only delay the first navigation in a newly created tab.
  if (!navigation_handle->GetWebContents()
           ->GetController()
           .IsInitialNavigation()) {
    return nullptr;
  }

  // TabUIHelper is initialized in TabHelpers::AttachTabHelpers for every tab
  // and is used to show customized favicon and title for the delayed tab. If
  // the corresponding TabUIHelper is null, it indicates that this WebContents
  // is not a tab, e.g., it can be a BrowserPlugin.
  if (!TabUIHelper::FromWebContents(navigation_handle->GetWebContents()))
    return nullptr;

  return base::MakeUnique<BackgroundTabNavigationThrottle>(navigation_handle);
}

const char* BackgroundTabNavigationThrottle::GetNameForLogging() {
  return "BackgroundTabNavigationThrottle";
}

BackgroundTabNavigationThrottle::BackgroundTabNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

BackgroundTabNavigationThrottle::~BackgroundTabNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
BackgroundTabNavigationThrottle::WillStartRequest() {
  return g_browser_process->GetTabManager()->MaybeThrottleNavigation(this);
}

void BackgroundTabNavigationThrottle::ResumeNavigation() {
  Resume();
}

}  // namespace resource_coordinator

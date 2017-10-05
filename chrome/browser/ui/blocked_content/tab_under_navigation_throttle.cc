// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"
#include "url/origin.h"

const base::Feature TabUnderNavigationThrottle::kBlockTabUnders{
    "BlockTabUnders", base::FEATURE_DISABLED_BY_DEFAULT};

// static
std::unique_ptr<content::NavigationThrottle>
TabUnderNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (handle->IsInMainFrame())
    return base::WrapUnique(new TabUnderNavigationThrottle(handle));
  return nullptr;
}

TabUnderNavigationThrottle::~TabUnderNavigationThrottle() = default;

TabUnderNavigationThrottle::TabUnderNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle),
      block_(base::FeatureList::IsEnabled(kBlockTabUnders)) {}

// static
bool TabUnderNavigationThrottle::IsSuspiciousClientRedirect(
    content::NavigationHandle* navigation_handle,
    bool started_in_background) {
  // Some browser initiated navigations have HasUserGesture set to false. This
  // should eventually be fixed in crbug.com/617904. In the meantime, just dont
  // block browser initiated ones.
  if (!started_in_background || !navigation_handle->IsInMainFrame() ||
      navigation_handle->HasUserGesture() ||
      !navigation_handle->IsRendererInitiated()) {
    return false;
  }

  // An empty previous URL indicates this was the first load. We filter these
  // out because we're primarily interested in sites which navigate themselves
  // away while in the background.
  const GURL& previous_main_frame_url =
      navigation_handle->HasCommitted()
          ? navigation_handle->GetPreviousURL()
          : navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (previous_main_frame_url.is_empty())
    return false;

  // Only cross origin navigations are considered tab-unders.
  if (url::Origin(previous_main_frame_url)
          .IsSameOriginWith(url::Origin(navigation_handle->GetURL()))) {
    return false;
  }

  return true;
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::MaybeBlockNavigation() {
  content::WebContents* contents = navigation_handle()->GetWebContents();
  content::WebContentsDelegate* delegate = contents->GetDelegate();
  auto* popup_opener = PopupOpenerTabHelper::FromWebContents(contents);

  if (delegate && popup_opener &&
      popup_opener->has_opened_popup_since_last_user_gesture() &&
      IsSuspiciousClientRedirect(navigation_handle(), started_in_background_)) {
    popup_opener->OnDidTabUnder();
    LogAction(Action::kDidTabUnder);

    if (block_) {
      LogAction(Action::kBlocked);
      delegate->OnDidBlockFramebust(contents, navigation_handle()->GetURL());
      return content::NavigationThrottle::CANCEL;
    }
  }
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::WillStartRequest() {
  LogAction(Action::kStarted);
  started_in_background_ = !navigation_handle()->GetWebContents()->IsVisible();
  return MaybeBlockNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::WillRedirectRequest() {
  return MaybeBlockNavigation();
}

const char* TabUnderNavigationThrottle::GetNameForLogging() {
  return "TabUnderNavigationThrottle";
}

void TabUnderNavigationThrottle::LogAction(Action action) const {
  UMA_HISTOGRAM_ENUMERATION("Tab.TabUnderAction", action, Action::kCount);
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/infobars/framebust_block_infobar.h"
#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"
#endif  // defined(OS_ANDROID)

namespace {

void LogAction(TabUnderNavigationThrottle::Action action) {
  UMA_HISTOGRAM_ENUMERATION("Tab.TabUnderAction", action,
                            TabUnderNavigationThrottle::Action::kCount);
}

void ShowUI(content::WebContents* web_contents,
            const GURL& url,
            base::OnceClosure click_closure) {
#if defined(OS_ANDROID)
  FramebustBlockInfoBar::Show(web_contents,
                              base::MakeUnique<FramebustBlockMessageDelegate>(
                                  web_contents, url, std::move(click_closure)));
#else
  NOTIMPLEMENTED() << "The BlockTabUnders experiment does not currently have a "
                      "UI implemented on desktop platforms.";
#endif
}

}  // namespace

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
  if (url::Origin::Create(previous_main_frame_url)
          .IsSameOriginWith(url::Origin::Create(navigation_handle->GetURL()))) {
    return false;
  }

  return true;
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::MaybeBlockNavigation() {
  content::WebContents* contents = navigation_handle()->GetWebContents();
  auto* popup_opener = PopupOpenerTabHelper::FromWebContents(contents);

  if (!seen_tab_under_ && popup_opener &&
      popup_opener->has_opened_popup_since_last_user_gesture() &&
      IsSuspiciousClientRedirect(navigation_handle(), started_in_background_)) {
    seen_tab_under_ = true;
    popup_opener->OnDidTabUnder();
    LogAction(Action::kDidTabUnder);

    if (block_) {
      LogAction(Action::kBlocked);
      ShowUI(contents, navigation_handle()->GetURL(),
             base::BindOnce(&LogAction, Action::kClickedThrough));
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

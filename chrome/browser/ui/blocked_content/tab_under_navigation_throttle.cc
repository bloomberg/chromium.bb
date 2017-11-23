// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "components/rappor/public/rappor_parameters.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/console_message_level.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
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

#if defined(OS_ANDROID)
typedef FramebustBlockMessageDelegate::InterventionOutcome InterventionOutcome;

void LogOutcome(InterventionOutcome outcome) {
  TabUnderNavigationThrottle::Action action;
  switch (outcome) {
    case InterventionOutcome::kAccepted:
      action = TabUnderNavigationThrottle::Action::kAcceptedIntervention;
      break;
    case InterventionOutcome::kDeclinedAndNavigated:
      action = TabUnderNavigationThrottle::Action::kClickedThrough;
      break;
  }
  LogAction(action);
}
#endif  // defined(OS_ANDROID)

void LogTabUnderAttempt(content::NavigationHandle* handle,
                        base::Optional<ukm::SourceId> opener_source_id) {
  LogAction(TabUnderNavigationThrottle::Action::kDidTabUnder);

  // Log RAPPOR / UKM based on the opener URL, not the URL navigated to.
  const GURL& opener_url = handle->GetWebContents()->GetLastCommittedURL();
  if (rappor::RapporService* rappor_service =
          g_browser_process->rappor_service()) {
    rappor_service->RecordSampleString(
        "Tab.TabUnder.Opener", rappor::UMA_RAPPOR_TYPE,
        rappor::GetDomainAndRegistrySampleFromGURL(opener_url));
  }

  // The source id should generally be set, except for very rare circumstances
  // where the popup opener tab helper is not observing at the time the
  // previous navigation commit.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (opener_source_id && ukm_recorder) {
    ukm::builders::AbusiveExperienceHeuristic(opener_source_id.value())
        .SetDidTabUnder(true)
        .Record(ukm_recorder);
  }
}

void ShowUI(content::WebContents* web_contents, const GURL& url) {
#if defined(OS_ANDROID)
  FramebustBlockInfoBar::Show(
      web_contents, base::MakeUnique<FramebustBlockMessageDelegate>(
                        web_contents, url, base::BindOnce(&LogOutcome)));
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
    LogTabUnderAttempt(navigation_handle(),
                       popup_opener->last_committed_source_id());

    if (block_) {
      const GURL& url = navigation_handle()->GetURL();
      const std::string error =
          base::StringPrintf(kBlockTabUnderFormatMessage, url.spec().c_str());
      contents->GetMainFrame()->AddMessageToConsole(
          content::CONSOLE_MESSAGE_LEVEL_ERROR, error.c_str());
      LogAction(Action::kBlocked);
      ShowUI(contents, url);
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

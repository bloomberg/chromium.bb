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
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/blocked_content/list_item_position.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "components/rappor/public/rappor_parameters.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/console_message_level.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/infobars/framebust_block_infobar.h"
#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"
#else
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#endif

namespace {

void LogAction(TabUnderNavigationThrottle::Action action, bool off_the_record) {
  UMA_HISTOGRAM_ENUMERATION("Tab.TabUnderAction", action,
                            TabUnderNavigationThrottle::Action::kCount);
  if (off_the_record) {
    UMA_HISTOGRAM_ENUMERATION("Tab.TabUnderAction.OTR", action,
                              TabUnderNavigationThrottle::Action::kCount);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Tab.TabUnderAction.NonOTR", action,
                              TabUnderNavigationThrottle::Action::kCount);
  }
}

#if defined(OS_ANDROID)
typedef FramebustBlockMessageDelegate::InterventionOutcome InterventionOutcome;

void LogOutcome(bool off_the_record, InterventionOutcome outcome) {
  TabUnderNavigationThrottle::Action action;
  switch (outcome) {
    case InterventionOutcome::kAccepted:
      action = TabUnderNavigationThrottle::Action::kAcceptedIntervention;
      break;
    case InterventionOutcome::kDeclinedAndNavigated:
      action = TabUnderNavigationThrottle::Action::kClickedThrough;
      break;
  }
  LogAction(action, off_the_record);
}
#else
void OnListItemClicked(bool off_the_record,
                       const GURL& url,
                       size_t index,
                       size_t total_size) {
  LogAction(TabUnderNavigationThrottle::Action::kClickedThrough,
            off_the_record);
  UMA_HISTOGRAM_ENUMERATION("Tab.TabUnder.ClickThroughPosition",
                            GetListItemPositionFromDistance(index, total_size),
                            ListItemPosition::kLast);
}
#endif

void LogTabUnderAttempt(content::NavigationHandle* handle,
                        base::Optional<ukm::SourceId> opener_source_id,
                        bool off_the_record) {
  LogAction(TabUnderNavigationThrottle::Action::kDidTabUnder, off_the_record);

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
      off_the_record_(
          handle->GetWebContents()->GetBrowserContext()->IsOffTheRecord()),
      block_(base::FeatureList::IsEnabled(kBlockTabUnders)),
      has_opened_popup_since_last_user_gesture_at_start_(
          HasOpenedPopupSinceLastUserGesture()) {}

// static
bool TabUnderNavigationThrottle::IsSuspiciousClientRedirect(
    content::NavigationHandle* navigation_handle) {
  // Some browser initiated navigations have HasUserGesture set to false. This
  // should eventually be fixed in crbug.com/617904. In the meantime, just dont
  // block browser initiated ones.
  if (!navigation_handle->IsInMainFrame() ||
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
  if (seen_tab_under_ || !has_opened_popup_since_last_user_gesture_at_start_ ||
      !IsSuspiciousClientRedirect(navigation_handle())) {
    return content::NavigationThrottle::PROCEED;
  }

  seen_tab_under_ = true;
  content::WebContents* contents = navigation_handle()->GetWebContents();
  auto* popup_opener = PopupOpenerTabHelper::FromWebContents(contents);
  DCHECK(popup_opener);
  popup_opener->OnDidTabUnder();

  LogTabUnderAttempt(navigation_handle(),
                     popup_opener->last_committed_source_id(), off_the_record_);

  if (block_) {
    const std::string error =
        base::StringPrintf(kBlockTabUnderFormatMessage,
                           navigation_handle()->GetURL().spec().c_str());
    contents->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR, error.c_str());
    LogAction(Action::kBlocked, off_the_record_);
    ShowUI();
    return content::NavigationThrottle::CANCEL;
  }
  return content::NavigationThrottle::PROCEED;
}

void TabUnderNavigationThrottle::ShowUI() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  const GURL& url = navigation_handle()->GetURL();
  bool off_the_record = web_contents->GetBrowserContext()->IsOffTheRecord();
#if defined(OS_ANDROID)
  FramebustBlockInfoBar::Show(
      web_contents,
      std::make_unique<FramebustBlockMessageDelegate>(
          web_contents, url, base::BindOnce(&LogOutcome, off_the_record)));
#else
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  DCHECK(content_settings);
  content_settings->OnFramebustBlocked(
      url, base::BindOnce(&OnListItemClicked, off_the_record));
#endif
}

bool TabUnderNavigationThrottle::HasOpenedPopupSinceLastUserGesture() const {
  content::WebContents* contents = navigation_handle()->GetWebContents();
  auto* popup_opener = PopupOpenerTabHelper::FromWebContents(contents);
  return popup_opener &&
         popup_opener->has_opened_popup_since_last_user_gesture();
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::WillStartRequest() {
  LogAction(Action::kStarted, off_the_record_);
  return MaybeBlockNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
TabUnderNavigationThrottle::WillRedirectRequest() {
  return MaybeBlockNavigation();
}

const char* TabUnderNavigationThrottle::GetNameForLogging() {
  return "TabUnderNavigationThrottle";
}

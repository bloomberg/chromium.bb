// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace safe_browsing {

namespace {

// Given when an event happened and its TTL, determine if it is already expired.
// Note, if for some reason this event's timestamp is in the future, this
// event's timestamp is invalid, hence we treat it as expired.
bool IsEventExpired(const base::Time& event_time, double ttl_in_second) {
  double current_time_in_second = base::Time::Now().ToDoubleT();
  double event_time_in_second = event_time.ToDoubleT();
  if (current_time_in_second <= event_time_in_second)
    return true;
  return current_time_in_second - event_time_in_second > ttl_in_second;
}

// Helper function to determine if the URL type should be LANDING_REFERRER or
// LANDING_PAGE, and modify AttributionResult accordingly.
ReferrerChainEntry::URLType GetURLTypeAndAdjustAttributionResult(
    bool at_user_gesture_limit,
    SafeBrowsingNavigationObserverManager::AttributionResult* out_result) {
  // Landing page of a download refers to the page user directly interacts
  // with to trigger this download (e.g. clicking on download button). Landing
  // referrer page is the one user interacts with right before navigating to
  // the landing page.
  // Since we are tracing navigations backwards, if we've reached
  // user gesture limit before this navigation event, this is a navigation
  // leading to the landing referrer page, otherwise it leads to landing page.
  if (at_user_gesture_limit) {
    *out_result =
        SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER;
    return ReferrerChainEntry::LANDING_REFERRER;
  } else {
    *out_result = SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE;
    return ReferrerChainEntry::LANDING_PAGE;
  }
}

}  // namespace

// The expiration period of a user gesture. Any user gesture that happened 1.0
// second ago is considered as expired and not relevant to upcoming navigation
// events.
static const double kUserGestureTTLInSecond = 1.0;
// The expiration period of navigation events and resolved IP addresses. Any
// navigation related records that happened 2 minutes ago are considered as
// expired. So we clean up these navigation footprints every 2 minutes.
static const double kNavigationFootprintTTLInSecond = 120.0;

// static
const base::Feature
SafeBrowsingNavigationObserverManager::kDownloadAttribution {
    "DownloadAttribution", base::FEATURE_DISABLED_BY_DEFAULT
};
// static
bool SafeBrowsingNavigationObserverManager::IsUserGestureExpired(
    const base::Time& timestamp) {
  return IsEventExpired(timestamp, kUserGestureTTLInSecond);
}

// static
GURL SafeBrowsingNavigationObserverManager::ClearEmptyRef(const GURL& url) {
  if (url.has_ref() && url.ref().empty()) {
    url::Replacements<char> replacements;
    replacements.ClearRef();
    return url.ReplaceComponents(replacements);
  }
  return url;
}

// static
bool SafeBrowsingNavigationObserverManager::IsEnabledAndReady(
    Profile* profile) {
  return base::FeatureList::IsEnabled(
      SafeBrowsingNavigationObserverManager::kDownloadAttribution) &&
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
      g_browser_process->safe_browsing_service() &&
      g_browser_process->safe_browsing_service()->navigation_observer_manager();
}

SafeBrowsingNavigationObserverManager::SafeBrowsingNavigationObserverManager() {
  registrar_.Add(this, chrome::NOTIFICATION_RETARGETING,
                 content::NotificationService::AllSources());

  // Schedule clean up in 2 minutes.
  ScheduleNextCleanUpAfterInterval(
      base::TimeDelta::FromSecondsD(kNavigationFootprintTTLInSecond));
}

void SafeBrowsingNavigationObserverManager::RecordNavigationEvent(
    const GURL& nav_event_key,
    NavigationEvent* nav_event) {
  auto insertion_result = navigation_map_.insert(
      std::make_pair(nav_event_key, std::vector<NavigationEvent>()));

  insertion_result.first->second.push_back(std::move(*nav_event));
}

void SafeBrowsingNavigationObserverManager::RecordUserGestureForWebContents(
    content::WebContents* web_contents,
    const base::Time& timestamp) {
  auto insertion_result =
      user_gesture_map_.insert(std::make_pair(web_contents, timestamp));
  // Update the timestamp if entry already exists.
  if (!insertion_result.second)
    insertion_result.first->second = timestamp;
}

void SafeBrowsingNavigationObserverManager::OnUserGestureConsumed(
    content::WebContents* web_contents,
    const base::Time& timestamp) {
  auto it = user_gesture_map_.find(web_contents);
  // Remove entry from |user_gesture_map_| as a user_gesture is consumed by
  // a navigation event.
  if (it != user_gesture_map_.end() && timestamp >= it->second)
    user_gesture_map_.erase(it);
}

bool SafeBrowsingNavigationObserverManager::HasUserGesture(
    content::WebContents* web_contents) {
  if (!web_contents)
    return false;
  auto it = user_gesture_map_.find(web_contents);
  if (it != user_gesture_map_.end() &&
      !IsEventExpired(it->second, kUserGestureTTLInSecond)) {
    return true;
  }
  return false;
}

void SafeBrowsingNavigationObserverManager::RecordHostToIpMapping(
    const std::string& host,
    const std::string& ip) {
  auto insert_result = host_to_ip_map_.insert(
      std::make_pair(host, std::vector<ResolvedIPAddress>()));
  if (!insert_result.second) {
    // host_to_ip_map already contains this key.
    // If this IP is already in the vector, we update its timestamp.
    for (auto& vector_entry : insert_result.first->second) {
      if (vector_entry.ip == host) {
        vector_entry.timestamp = base::Time::Now();
        return;
      }
    }
  }
  // If this is a new IP of this host, and we added to the end of the vector.
  insert_result.first->second.push_back(
      ResolvedIPAddress(base::Time::Now(), ip));
}

void SafeBrowsingNavigationObserverManager::OnWebContentDestroyed(
    content::WebContents* web_contents) {
  user_gesture_map_.erase(web_contents);
}

void SafeBrowsingNavigationObserverManager::CleanUpStaleNavigationFootprints() {
  CleanUpNavigationEvents();
  CleanUpUserGestures();
  CleanUpIpAddresses();
  ScheduleNextCleanUpAfterInterval(
      base::TimeDelta::FromSecondsD(kNavigationFootprintTTLInSecond));
}

SafeBrowsingNavigationObserverManager::AttributionResult
SafeBrowsingNavigationObserverManager::IdentifyReferrerChainForDownload(
    const GURL& target_url,
    int target_tab_id,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain) {
  if (!target_url.is_valid())
    return INVALID_URL;

  NavigationEvent* nav_event = FindNavigationEvent(
      target_url,
      GURL(),
      target_tab_id);
  if (!nav_event) {
    // We cannot find a single navigation event related to this download.
    return NAVIGATION_EVENT_NOT_FOUND;
  }
  AttributionResult result = SUCCESS;
  AddToReferrerChain(out_referrer_chain, nav_event,
                     ReferrerChainEntry::DOWNLOAD_URL);
  int user_gesture_count = 0;
  GetRemainingReferrerChain(
      nav_event,
      user_gesture_count,
      user_gesture_count_limit,
      out_referrer_chain,
      &result);
  return result;
}

SafeBrowsingNavigationObserverManager::AttributionResult
SafeBrowsingNavigationObserverManager::IdentifyReferrerChainForPPAPIDownload(
    const GURL& initiating_frame_url,
    int tab_id,
    bool has_user_gesture,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain) {
  if (!initiating_frame_url.is_valid())
    return INVALID_URL;

  NavigationEvent* nav_event =
      FindNavigationEvent(initiating_frame_url, GURL(), tab_id);
  if (!nav_event) {
    // We cannot find a single navigation event related to this download.
    return NAVIGATION_EVENT_NOT_FOUND;
  }

  AttributionResult result = SUCCESS;

  int user_gesture_count = 0;
  // If this initiating_frame has user gesture, we consider this as the landing
  // page of the PPAPI download.
  if (has_user_gesture) {
    user_gesture_count = 1;
    AddToReferrerChain(out_referrer_chain, nav_event,
                       GetURLTypeAndAdjustAttributionResult(
                          user_gesture_count == user_gesture_count_limit,
                          &result));
  } else {
    AddToReferrerChain(out_referrer_chain, nav_event,
                       nav_event->has_server_redirect
                           ? ReferrerChainEntry::SERVER_REDIRECT
                           : ReferrerChainEntry::CLIENT_REDIRECT);
  }

  GetRemainingReferrerChain(
      nav_event,
      user_gesture_count,
      user_gesture_count_limit,
      out_referrer_chain,
      &result);
  return result;
}

SafeBrowsingNavigationObserverManager::
    ~SafeBrowsingNavigationObserverManager() {}

void SafeBrowsingNavigationObserverManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_RETARGETING)
    RecordRetargeting(details);
}

void SafeBrowsingNavigationObserverManager::RecordRetargeting(
    const content::NotificationDetails& details) {
  const RetargetingDetails* retargeting_detail =
      content::Details<const RetargetingDetails>(details).ptr();
  DCHECK(retargeting_detail);
  content::WebContents* source_contents =
      retargeting_detail->source_web_contents;
  content::WebContents* target_contents =
      retargeting_detail->target_web_contents;
  DCHECK(source_contents);
  DCHECK(target_contents);

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      retargeting_detail->source_render_process_id,
      retargeting_detail->source_render_frame_id);
  // Remove the "#" at the end of URL, since it does not point to any actual
  // page fragment ID.
  GURL target_url = SafeBrowsingNavigationObserverManager::ClearEmptyRef(
      retargeting_detail->target_url);

  NavigationEvent nav_event;
  if (rfh) {
    nav_event.source_url = SafeBrowsingNavigationObserverManager::ClearEmptyRef(
        rfh->GetLastCommittedURL());
  }
  nav_event.source_tab_id = SessionTabHelper::IdForTab(source_contents);
  nav_event.source_main_frame_url =
      SafeBrowsingNavigationObserverManager::ClearEmptyRef(
          source_contents->GetLastCommittedURL());
  nav_event.original_request_url = target_url;
  nav_event.destination_url = target_url;
  nav_event.target_tab_id = SessionTabHelper::IdForTab(target_contents);
  nav_event.frame_id = rfh ? rfh->GetFrameTreeNodeId() : -1;
  auto it = user_gesture_map_.find(source_contents);
  if (it != user_gesture_map_.end() &&
      !SafeBrowsingNavigationObserverManager::IsUserGestureExpired(
          it->second)) {
    nav_event.is_user_initiated = true;
    OnUserGestureConsumed(it->first, it->second);
  } else {
    nav_event.is_user_initiated = false;
  }

  auto insertion_result = navigation_map_.insert(
      std::make_pair(target_url, std::vector<NavigationEvent>()));
  insertion_result.first->second.push_back(std::move(nav_event));
}

void SafeBrowsingNavigationObserverManager::CleanUpNavigationEvents() {
  // Remove any stale NavigationEnvent, if it is older than
  // kNavigationFootprintTTLInSecond.
  for (auto it = navigation_map_.begin(); it != navigation_map_.end();) {
    it->second.erase(std::remove_if(it->second.begin(), it->second.end(),
                                    [](const NavigationEvent& nav_event) {
                                      return IsEventExpired(
                                          nav_event.last_updated,
                                          kNavigationFootprintTTLInSecond);
                                    }),
                     it->second.end());
    if (it->second.size() == 0)
      it = navigation_map_.erase(it);
    else
      ++it;
  }
}

void SafeBrowsingNavigationObserverManager::CleanUpUserGestures() {
  for (auto it = user_gesture_map_.begin(); it != user_gesture_map_.end();) {
    if (IsEventExpired(it->second, kUserGestureTTLInSecond))
      it = user_gesture_map_.erase(it);
    else
      ++it;
  }
}

void SafeBrowsingNavigationObserverManager::CleanUpIpAddresses() {
  for (auto it = host_to_ip_map_.begin(); it != host_to_ip_map_.end();) {
    it->second.erase(std::remove_if(it->second.begin(), it->second.end(),
                                    [](const ResolvedIPAddress& resolved_ip) {
                                      return IsEventExpired(
                                          resolved_ip.timestamp,
                                          kNavigationFootprintTTLInSecond);
                                    }),
                     it->second.end());
    if (it->second.size() == 0)
      it = host_to_ip_map_.erase(it);
    else
      ++it;
  }
}

bool SafeBrowsingNavigationObserverManager::IsCleanUpScheduled() const {
  return cleanup_timer_.IsRunning();
}

void SafeBrowsingNavigationObserverManager::ScheduleNextCleanUpAfterInterval(
    base::TimeDelta interval) {
  DCHECK_GT(interval, base::TimeDelta());
  cleanup_timer_.Stop();
  cleanup_timer_.Start(
      FROM_HERE, interval, this,
      &SafeBrowsingNavigationObserverManager::CleanUpStaleNavigationFootprints);
}

NavigationEvent* SafeBrowsingNavigationObserverManager::FindNavigationEvent(
    const GURL& target_url,
    const GURL& target_main_frame_url,
    int target_tab_id) {
  if (target_url.is_empty() && target_main_frame_url.is_empty())
    return nullptr;

  // If target_url is empty, we should back trace navigation based on its
  // main frame URL instead.
  GURL search_url =
      target_url.is_empty() ? target_main_frame_url : target_url;
  auto it = navigation_map_.find(search_url);
  if (it == navigation_map_.end())
    return nullptr;

  // Since navigation events are recorded in chronological order, we traverse
  // the vector in reverse order to get the latest match.
  for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
    // If tab id is not valid, we only compare url, otherwise we compare both.
    if (rit->destination_url == search_url &&
        (target_tab_id == -1 || rit->target_tab_id == target_tab_id)) {
      // If both source_url and source_main_frame_url are empty, and this
      // navigation is not triggered by user, a retargeting navigation probably
      // causes this navigation. In this case, we skip this navigation event and
      // looks for the retargeting navigation event.
      if (rit->source_url.is_empty() && rit->source_main_frame_url.is_empty() &&
          !rit->is_user_initiated) {
        // If there is a server redirection immediately after retargeting, we
        // need to adjust our search url to the original request.
        if (rit->has_server_redirect){
          NavigationEvent* retargeting_nav_event =
              FindNavigationEvent(rit->original_request_url,
                                  GURL(),
                                  rit->target_tab_id);
          if (!retargeting_nav_event)
            return nullptr;
          // Adjust retargeting navigation event's attributes.
          retargeting_nav_event->has_server_redirect = true;
          retargeting_nav_event->destination_url = search_url;
          return retargeting_nav_event;
        } else {
          continue;
        }
      } else {
        return &*rit;
      }
    }
  }
  return nullptr;
}

void SafeBrowsingNavigationObserverManager::AddToReferrerChain(
    ReferrerChain* referrer_chain,
    NavigationEvent* nav_event,
    ReferrerChainEntry::URLType type) {
  std::unique_ptr<ReferrerChainEntry> referrer_chain_entry =
      base::MakeUnique<ReferrerChainEntry>();
  referrer_chain_entry->set_url(nav_event->destination_url.spec());
  referrer_chain_entry->set_type(type);
  auto ip_it = host_to_ip_map_.find(nav_event->destination_url.host());
  if (ip_it != host_to_ip_map_.end()) {
    for (ResolvedIPAddress entry : ip_it->second) {
      referrer_chain_entry->add_ip_addresses(entry.ip);
    }
  }
  // Since we only track navigation to landing referrer, we will not log the
  // referrer of the landing referrer page.
  if (type != ReferrerChainEntry::LANDING_REFERRER) {
    referrer_chain_entry->set_referrer_url(nav_event->source_url.spec());
    referrer_chain_entry->set_referrer_main_frame_url(
        nav_event->source_main_frame_url.spec());
  }
  referrer_chain_entry->set_is_retargeting(nav_event->source_tab_id !=
                                          nav_event->target_tab_id);
  referrer_chain_entry->set_navigation_time_msec(
      nav_event->last_updated.ToJavaTime());
  referrer_chain->push_back(std::move(referrer_chain_entry));
}

void SafeBrowsingNavigationObserverManager::GetRemainingReferrerChain(
    NavigationEvent* last_nav_event_traced,
    int current_user_gesture_count,
    int user_gesture_count_limit,
    ReferrerChain* out_referrer_chain,
    SafeBrowsingNavigationObserverManager::AttributionResult* out_result) {

  while (current_user_gesture_count < user_gesture_count_limit) {
    // Back trace to the next nav_event that was initiated by the user.
    while (!last_nav_event_traced->is_user_initiated) {
      last_nav_event_traced =
          FindNavigationEvent(last_nav_event_traced->source_url,
                              last_nav_event_traced->source_main_frame_url,
                              last_nav_event_traced->source_tab_id);
      if (!last_nav_event_traced)
        return;
      AddToReferrerChain(out_referrer_chain, last_nav_event_traced,
                         last_nav_event_traced->has_server_redirect
                             ? ReferrerChainEntry::SERVER_REDIRECT
                             : ReferrerChainEntry::CLIENT_REDIRECT);
    }

    current_user_gesture_count++;


    // If the source_url and source_main_frame_url of current navigation event
    // are empty, and is_user_initiated is true, this is a browser initiated
    // navigation (e.g. trigged by typing in address bar, clicking on bookmark,
    // etc). We reached the end of the referrer chain.
    if (last_nav_event_traced->source_url.is_empty() &&
        last_nav_event_traced->source_main_frame_url.is_empty()) {
      DCHECK(last_nav_event_traced->is_user_initiated);
      return;
    }

    last_nav_event_traced =
        FindNavigationEvent(last_nav_event_traced->source_url,
                            last_nav_event_traced->source_main_frame_url,
                            last_nav_event_traced->source_tab_id);
    if (!last_nav_event_traced)
      return;

    AddToReferrerChain(out_referrer_chain, last_nav_event_traced,
                       GetURLTypeAndAdjustAttributionResult(
                          current_user_gesture_count ==
                              user_gesture_count_limit,
                          out_result));
  }
}

}  // namespace safe_browsing

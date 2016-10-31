// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace safe_browsing {

// The expiration period of a user gesture. Any user gesture that happened 1.0
// second ago will be considered as expired and not relevant to upcoming
// navigation events.
static const double kUserGestureTTLInSecond = 1.0;

// static
bool SafeBrowsingNavigationObserverManager::IsUserGestureExpired(
    const base::Time& timestamp) {
  double now = base::Time::Now().ToDoubleT();
  double timestamp_in_double = timestamp.ToDoubleT();

  if (now <= timestamp_in_double)
    return true;
  return (now - timestamp_in_double) > kUserGestureTTLInSecond;
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

SafeBrowsingNavigationObserverManager::SafeBrowsingNavigationObserverManager() {
  registrar_.Add(this, chrome::NOTIFICATION_RETARGETING,
                 content::NotificationService::AllSources());
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
  // TODO (jialiul): Will add other clean up tasks shortly.
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

}  // namespace safe_browsing

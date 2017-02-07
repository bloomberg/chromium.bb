// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_H_

#include "base/supports_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

namespace safe_browsing {
class SafeBrowsingNavigationObserverManager;

// Struct to record the details of a navigation event for any frame.
// This information will be used to fill |url_chain| field in safe browsing
// download pings.
struct NavigationEvent {
  NavigationEvent();
  NavigationEvent(NavigationEvent&& nav_event);
  NavigationEvent& operator=(NavigationEvent&& nav_event);
  ~NavigationEvent();

  GURL source_url;  // URL that caused this navigation to occur.
                    // TODO(jialiul): source_url may be incorrect when
                    // navigation involves frames targeting each other.
                    // http://crbug.com/651895.
  GURL source_main_frame_url;  // Main frame url of the source_url. Could be the
                               // same as source_url, if source_url was loaded
                               // in main frame.
  GURL original_request_url;   // The original request URL of this navigation.
  std::vector<GURL> server_redirect_urls;  // Server redirect url chain.
                                           // Empty if there is no server
                                           // redirect. If set, last url in this
                                           // vector is the destination url.
  int source_tab_id;  // Which tab contains the frame with source_url. Tab ID is
                      // returned by SessionTabHelper::IdForTab. This ID is
                      // immutable for a given tab and unique across Chrome
                      // within the current session.
  int target_tab_id;  // Which tab this request url is targeting to.
  int frame_id;  // Frame tree node ID of the frame where this navigation takes
                 // place.
  base::Time last_updated;  // When this NavigationEvent was last updated.
  bool is_user_initiated;  // browser_initiated || has_user_gesture.
  bool has_committed;

  const GURL& GetDestinationUrl() const {
    if (!server_redirect_urls.empty())
      return server_redirect_urls.back();
    else
      return original_request_url;
  }
};

// Structure to keep track of resolved IP address of a host.
struct ResolvedIPAddress {
  ResolvedIPAddress() : timestamp(base::Time::Now()), ip() {}
  ResolvedIPAddress(base::Time timestamp, const std::string& ip)
      : timestamp(timestamp), ip(ip) {}
  base::Time timestamp;  // Timestamp of when we get the resolved IP.
  std::string ip;        // Resolved IP address
};

// Observes the navigation events for a single WebContents (both main-frame
// and sub-frame navigations).
class SafeBrowsingNavigationObserver : public base::SupportsUserData::Data,
                                       public content::WebContentsObserver {
 public:
  static void MaybeCreateForWebContents(
      content::WebContents* web_contents);

  static SafeBrowsingNavigationObserver* FromWebContents(
      content::WebContents* web_contents);

  SafeBrowsingNavigationObserver(
      content::WebContents* contents,
      const scoped_refptr<SafeBrowsingNavigationObserverManager>& manager);

  ~SafeBrowsingNavigationObserver() override;

 private:
  typedef std::unordered_map<content::NavigationHandle*,
                             std::unique_ptr<NavigationEvent>>
      NavigationHandleMap;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetResourceResponseStart(
      const content::ResourceRequestDetails& details) override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;
  void WebContentsDestroyed() override;

  // Map keyed on NavigationHandle* to keep track of all the ongoing navigation
  // events. NavigationHandle pointers are owned by RenderFrameHost. Since a
  // NavigationHandle object will be destructed after navigation is done,
  // at the end of DidFinishNavigation(...) corresponding entries in this map
  // will be removed from navigation_handle_map_ and added to
  // SafeBrowsingNavigationObserverManager::navigation_map_.
  NavigationHandleMap navigation_handle_map_;

  scoped_refptr<SafeBrowsingNavigationObserverManager> manager_;

  // If the observed WebContents just got an user gesture.
  bool has_user_gesture_;

  base::Time last_user_gesture_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingNavigationObserver);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_H_

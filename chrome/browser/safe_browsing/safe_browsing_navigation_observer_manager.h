// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_

#include "base/feature_list.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {

class SafeBrowsingNavigationObserver;
struct NavigationEvent;
struct ResolvedIPAddress;

// Manager class for SafeBrowsingNavigationObserver, which is in charge of
// cleaning up stale navigation events, and identifying landing page/landing
// referrer for a specific download.
// TODO(jialiul): For now, SafeBrowsingNavigationObserverManager also listens to
// NOTIFICATION_RETARGETING as a way to detect cross frame/tab navigation.
// Remove base class content::NotificationObserver when
// WebContentsObserver::DidOpenRequestedURL() covers all retargeting cases.
class SafeBrowsingNavigationObserverManager
    : public content::NotificationObserver,
      public base::RefCountedThreadSafe<SafeBrowsingNavigationObserverManager> {
 public:
  static const base::Feature kDownloadAttribution;
  typedef std::vector<std::unique_ptr<ReferrerChainEntry>> ReferrerChain;

  // For UMA histogram counting. Do NOT change order.
  enum AttributionResult {
    SUCCESS = 1,                   // Identified referrer chain is not empty.
    SUCCESS_LANDING_PAGE = 2,      // Successfully identified landing page.
    SUCCESS_LANDING_REFERRER = 3,  // Successfully identified landing referrer.
    INVALID_URL = 4,
    NAVIGATION_EVENT_NOT_FOUND = 5,

    // Always at the end.
    ATTRIBUTION_FAILURE_TYPE_MAX
  };

  // Helper function to check if user gesture is older than
  // kUserGestureTTLInSecond.
  static bool IsUserGestureExpired(const base::Time& timestamp);

  // Helper function to strip empty ref fragment from a URL. Many pages
  // end up with a "#" at the end of their URLs due to navigation triggered by
  // href="#" and javascript onclick function. We don't want to have separate
  // entries for these cases in the maps.
  static GURL ClearEmptyRef(const GURL& url);

  // Checks if we should enable observing navigations for safe browsing purpose.
  // Return true if the safe browsing service and the download attribution
  // feature are both enabled, and safe browsing service is initialized.
  static bool IsEnabledAndReady(Profile* profile);

  SafeBrowsingNavigationObserverManager();

  // Add |nav_event| to |navigation_map_| based on |nav_event_key|. Object
  // pointed to by |nav_event| will be no longer accessible after this function.
  void RecordNavigationEvent(const GURL& nav_event_key,
                             NavigationEvent* nav_event);
  void RecordUserGestureForWebContents(content::WebContents* web_contents,
                                       const base::Time& timestamp);
  void OnUserGestureConsumed(content::WebContents* web_contents,
                             const base::Time& timestamp);
  bool HasUserGesture(content::WebContents* web_contents);
  void RecordHostToIpMapping(const std::string& host, const std::string& ip);

  // Clean-ups need to be done when a WebContents gets destroyed.
  void OnWebContentDestroyed(content::WebContents* web_contents);

  // Remove all the observed NavigationEvents, user gestures, and resolved IP
  // addresses that are older than kNavigationFootprintTTLInSecond.
  void CleanUpStaleNavigationFootprints();

  // Based on the |target_url| and |target_tab_id|, trace back the observed
  // NavigationEvents in navigation_map_ to identify the sequence of navigations
  // leading to the target, with the coverage limited to
  // |user_gesture_count_limit| number of user gestures. Then convert these
  // identified NavigationEvents into ReferrerChainEntrys and append them to
  // |out_referrer_chain|.
  AttributionResult IdentifyReferrerChainForDownload(
      const GURL& target_url,
      int target_tab_id,  // -1 if tab id is not valid
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain);

  // Based on the |initiating_frame_url| and its associated |tab_id|, trace back
  // the observed NavigationEvents in navigation_map_ to identify the sequence
  // of navigations leading to this |initiating_frame_url|. If this initiating
  // frame has a user gesture, we trace back with the coverage limited to
  // |user_gesture_count_limit|-1 number of user gestures, otherwise we trace
  // back |user_gesture_count_limit| number of user gestures. We then convert
  // these identified NavigationEvents into ReferrerChainEntrys and append them
  // to |out_referrer_chain|.
  AttributionResult IdentifyReferrerChainForPPAPIDownload(
      const GURL& initiating_frame_url,
      int tab_id,
      bool has_user_gesture,
      int user_gesture_count_limit,
      ReferrerChain* out_referrer_chain);

 private:
  friend class base::RefCountedThreadSafe<
      SafeBrowsingNavigationObserverManager>;
  friend class TestNavigationObserverManager;
  friend class SBNavigationObserverBrowserTest;
  friend class SBNavigationObserverTest;

  struct GurlHash {
    std::size_t operator()(const GURL& url) const {
      return std::hash<std::string>()(url.spec());
    }
  };

  typedef std::unordered_map<GURL, std::vector<NavigationEvent>, GurlHash>
      NavigationMap;
  typedef std::unordered_map<content::WebContents*, base::Time> UserGestureMap;
  typedef std::unordered_map<std::string, std::vector<ResolvedIPAddress>>
      HostToIpMap;

  ~SafeBrowsingNavigationObserverManager() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void RecordRetargeting(const content::NotificationDetails& details);

  NavigationMap* navigation_map() { return &navigation_map_; }

  HostToIpMap* host_to_ip_map() { return &host_to_ip_map_; }

  // Remove stale entries from navigation_map_ if they are older than
  // kNavigationFootprintTTLInSecond (2 minutes).
  void CleanUpNavigationEvents();

  // Remove stale entries from user_gesture_map_ if they are older than
  // kUserGestureTTLInSecond (1 sec).
  void CleanUpUserGestures();

  // Remove stale entries from host_to_ip_map_ if they are older than
  // kNavigationFootprintTTLInSecond (2 minutes).
  void CleanUpIpAddresses();

  bool IsCleanUpScheduled() const;

  void ScheduleNextCleanUpAfterInterval(base::TimeDelta interval);

  // Find the most recent navigation event that navigated to |target_url| and
  // its associated |target_main_frame_url| in the tab with ID |target_tab_id|.
  // If navigation happened in the main frame, |target_url| and |target_main_
  // frame_url| are the same.
  // If |target_url| is empty, we use its main frame url (a.k.a.
  // |target_main_frame_url|) to search for navigation events.
  // If |target_tab_id| is not available (-1), we look for all tabs for the most
  // recent navigation to |target_url| or |target_main_frame_url|.
  // For some cases, the most recent navigation to |target_url| may not be
  // relevant.
  // For example, url1 in window A opens url2 in window B, url1 then opens an
  // about:blank page window C and injects script code in it to trigger a
  // delayed download in Window D. Before the download occurs, url2 in window B
  // opens a different about:blank page in window C.
  // A ---- C - D
  //   \   /
  //     B
  // In this case, FindNavigationEvent() will think url2 in Window B is the
  // referrer of about::blank in Window C since this navigation is more recent.
  // However, it does not prevent us to attribute url1 in Window A as the cause
  // of all these navigations.
  NavigationEvent* FindNavigationEvent(const GURL& target_url,
                                       const GURL& target_main_frame_url,
                                       int target_tab_id);

  void AddToReferrerChain(ReferrerChain* referrer_chain,
                          NavigationEvent* nav_event,
                          ReferrerChainEntry::URLType type);

  // Helper function to get the remaining referrer chain when we've already
  // traced back |current_user_gesture_count| number of user gestures.
  // This function modifies the |out_referrer_chain| and |out_result|.
  void GetRemainingReferrerChain(NavigationEvent* last_nav_event_traced,
                                 int current_user_gesture_count,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain,
                                 AttributionResult* out_result);

  // navigation_map_ keeps track of all the observed navigations. This map is
  // keyed on the resolved request url. In other words, in case of server
  // redirects, its key is the last server redirect url, otherwise, it is the
  // original target url. Since the same url can be requested multiple times
  // across different tabs and frames, the value of this map is a vector of
  // NavigationEvent ordered by navigation finish time.
  // Entries in navigation_map_ will be removed if they are older than 2 minutes
  // since their corresponding navigations finish.
  NavigationMap navigation_map_;

  // user_gesture_map_ keeps track of the timestamp of last user gesture in
  // in each WebContents. We assume for majority of cases, a navigation
  // shortly after a user gesture indicate this navigation is user initiated.
  UserGestureMap user_gesture_map_;

  // Host to timestamped IP addresses map that covers all the main frame and
  // subframe URLs' hosts. Since it is possible for a host to resolve to more
  // than one IP in even a short period of time, we map a single host to a
  // vector of ResolvedIPAddresss. This map is used to fill in ip_address field
  // in URLChainEntry in ClientDownloadRequest.
  HostToIpMap host_to_ip_map_;

  content::NotificationRegistrar registrar_;

  base::OneShotTimer cleanup_timer_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingNavigationObserverManager);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_

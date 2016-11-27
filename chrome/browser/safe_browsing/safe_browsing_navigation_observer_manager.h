// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace safe_browsing {

class SafeBrowsingNavigationObserver;
struct NavigationEvent;
struct ResolvedIPAddress;

// Manager class for SafeBrowsingNavigationObserver, which is in charge of
// cleaning up stale navigation events, and identifing landing page/landing
// referrer for a specific download.
// TODO(jialiul): For now, SafeBrowsingNavigationObserverManager also listens to
// NOTIFICATION_RETARGETING as a way to detect cross frame/tab navigation.
// Remove base class content::NotificationObserver when
// WebContentsObserver::DidOpenRequestedURL() covers all retargeting cases.
class SafeBrowsingNavigationObserverManager
    : public content::NotificationObserver,
      public base::RefCountedThreadSafe<SafeBrowsingNavigationObserverManager> {
 public:
  // Helper function to check if user gesture is older than
  // kUserGestureTTLInSecond.
  static bool IsUserGestureExpired(const base::Time& timestamp);
  // Helper function to strip empty ref fragment from a URL. Many pages
  // end up with a "#" at the end of their URLs due to navigation triggered by
  // href="#" and javascript onclick function. We don't want to have separate
  // entries for these cases in the maps.
  static GURL ClearEmptyRef(const GURL& url);

  SafeBrowsingNavigationObserverManager();

  // Add |nav_event| to |navigation_map_| based on |nav_event_key|. Object
  // pointed to by |nav_event| will be no longer accessible after this function.
  void RecordNavigationEvent(const GURL& nav_event_key,
                             NavigationEvent* nav_event);
  void RecordUserGestureForWebContents(content::WebContents* web_contents,
                                       const base::Time& timestamp);
  void OnUserGestureConsumed(content::WebContents* web_contents,
                             const base::Time& timestamp);
  void RecordHostToIpMapping(const std::string& host, const std::string& ip);
  // Clean-ups need to be done when a WebContents gets destroyed.
  void OnWebContentDestroyed(content::WebContents* web_contents);

  // TODO(jialiul): more functions are coming for managing navigation_map_.

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

  // navigation_map_ keeps track of all the observed navigations. This map is
  // keyed on the resolved request url. In other words, in case of server
  // redirects, its key is the last server redirect url, otherwise, it is the
  // original target url. Since the same url can be requested multiple times
  // across different tabs and frames, the value of this map is a vector of
  // NavigationEvent ordered by navigation finish time.
  // TODO(jialiul): Entries in navigation_map_ will be removed if they are older
  // than 2 minutes since their corresponding navigations finish.
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

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingNavigationObserverManager);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_

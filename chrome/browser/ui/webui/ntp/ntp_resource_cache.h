// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class RefCountedMemory;
}

// This class keeps a cache of NTP resources (HTML and CSS) so we don't have to
// regenerate them all the time.
class NTPResourceCache : public content::NotificationObserver,
                         public ProfileKeyedService {
 public:
  explicit NTPResourceCache(Profile* profile);
  virtual ~NTPResourceCache();

  base::RefCountedMemory* GetNewTabHTML(bool is_incognito);
  base::RefCountedMemory* GetNewTabCSS(bool is_incognito);

  void set_should_show_most_visited_page(bool should_show_most_visited_page) {
    should_show_most_visited_page_ = should_show_most_visited_page;
  }
  void set_should_show_other_devices_menu(bool should_show_other_devices_menu) {
    should_show_other_devices_menu_ = should_show_other_devices_menu;
  }
  void set_should_show_recently_closed_menu(
      bool should_show_recently_closed_menu) {
    should_show_recently_closed_menu_ = should_show_recently_closed_menu;
  }
  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void OnPreferenceChanged();

  void CreateNewTabHTML();

  // Helper to determine if the resource cache should be invalidated.
  // This is called on every page load, and can be used to check values that
  // don't generate a notification when changed (e.g., system preferences).
  bool NewTabCacheNeedsRefresh();

  Profile* profile_;

  scoped_refptr<base::RefCountedMemory> new_tab_html_;

#if !defined(OS_ANDROID)
  // Returns a message describing any newly-added sync types, or an empty
  // string if all types have already been acknowledged.
  string16 GetSyncTypeMessage();

  void CreateNewTabIncognitoHTML();

  void CreateNewTabIncognitoCSS();

  void CreateNewTabCSS();

  scoped_refptr<base::RefCountedMemory> new_tab_incognito_html_;
  scoped_refptr<base::RefCountedMemory> new_tab_incognito_css_;
  scoped_refptr<base::RefCountedMemory> new_tab_css_;
  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;
#endif

  // Set based on platform_util::IsSwipeTrackingFromScrollEventsEnabled.
  bool is_swipe_tracking_from_scroll_events_enabled_;
  // Set based on extensions::IsAppLauncherEnabled.
  bool should_show_apps_page_;
  // The next three all default to true and can be manually set, e.g., by the
  // chrome://apps page.
  bool should_show_most_visited_page_;
  bool should_show_other_devices_menu_;
  bool should_show_recently_closed_menu_;

  DISALLOW_COPY_AND_ASSIGN(NTPResourceCache);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_

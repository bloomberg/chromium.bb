// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
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

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  Profile* profile_;

  void CreateNewTabHTML();
  scoped_refptr<base::RefCountedMemory> new_tab_html_;

  // Helper to determine if the resource cache should be invalidated.
  // This is called on every page load, and can be used to check values that
  // don't generate a notification when changed (e.g., system preferences).
  bool NewTabCacheNeedsRefresh();

#if !defined(OS_ANDROID)
  // Returns a message describing any newly-added sync types, or an empty
  // string if all types have already been acknowledged.
  string16 GetSyncTypeMessage();

  void CreateNewTabIncognitoHTML();
  scoped_refptr<base::RefCountedMemory> new_tab_incognito_html_;

  void CreateNewTabIncognitoCSS();
  scoped_refptr<base::RefCountedMemory> new_tab_incognito_css_;
  void CreateNewTabCSS();
  scoped_refptr<base::RefCountedMemory> new_tab_css_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;
#endif

  bool is_swipe_tracking_from_scroll_events_enabled_;

  DISALLOW_COPY_AND_ASSIGN(NTPResourceCache);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Profile;
class RefCountedMemory;

// This class keeps a cache of NTP resources (HTML and CSS) so we don't have to
// regenerate them all the time.
class NTPResourceCache : public NotificationObserver,
                         public ProfileKeyedService {
 public:
  explicit NTPResourceCache(Profile* profile);
  virtual ~NTPResourceCache();

  RefCountedMemory* GetNewTabHTML(bool is_incognito);
  RefCountedMemory* GetNewTabCSS(bool is_incognito);

  // NotificationObserver interface.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  Profile* profile_;

  // Returns a message describing any newly-added sync types, or an empty
  // string if all types have already been acknowledged.
  string16 GetSyncTypeMessage();

  void CreateNewTabIncognitoHTML();
  scoped_refptr<RefCountedMemory> new_tab_incognito_html_;
  void CreateNewTabHTML();
  scoped_refptr<RefCountedMemory> new_tab_html_;

  void CreateNewTabIncognitoCSS();
  scoped_refptr<RefCountedMemory> new_tab_incognito_css_;
  void CreateNewTabCSS();
  scoped_refptr<RefCountedMemory> new_tab_css_;

  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(NTPResourceCache);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_

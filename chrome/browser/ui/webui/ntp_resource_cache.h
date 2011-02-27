// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_RESOURCE_CACHE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_RESOURCE_CACHE_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Profile;
class RefCountedBytes;

// This class keeps a cache of NTP resources (HTML and CSS) so we don't have to
// regenerate them all the time.
class NTPResourceCache : public NotificationObserver {
 public:
  explicit NTPResourceCache(Profile* profile);
  virtual ~NTPResourceCache();

  RefCountedBytes* GetNewTabHTML(bool is_off_the_record);
  RefCountedBytes* GetNewTabCSS(bool is_off_the_record);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  Profile* profile_;

  void CreateNewTabIncognitoHTML();
  scoped_refptr<RefCountedBytes> new_tab_incognito_html_;
  void CreateNewTabHTML();
  scoped_refptr<RefCountedBytes> new_tab_html_;

  void CreateNewTabIncognitoCSS();
  scoped_refptr<RefCountedBytes> new_tab_incognito_css_;
  void CreateNewTabCSS();
  scoped_refptr<RefCountedBytes> new_tab_css_;

  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(NTPResourceCache);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_RESOURCE_CACHE_H_

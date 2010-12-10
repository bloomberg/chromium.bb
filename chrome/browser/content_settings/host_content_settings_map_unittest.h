// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_UNITTEST_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_UNITTEST_H_
#pragma once

#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"

class HostContentSettingsMap;

class StubSettingsObserver : public NotificationObserver {
 public:
  StubSettingsObserver() : last_notifier(NULL), counter(0) {
    registrar_.Add(this, NotificationType::CONTENT_SETTINGS_CHANGED,
                   NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    ++counter;
    Source<HostContentSettingsMap> content_settings(source);
    Details<ContentSettingsDetails> settings_details(details);
    last_notifier = content_settings.ptr();
    last_pattern = settings_details.ptr()->pattern();
    last_update_all = settings_details.ptr()->update_all();
    last_update_all_types = settings_details.ptr()->update_all_types();
    last_type = settings_details.ptr()->type();
    // This checks that calling a Get function from an observer doesn't
    // deadlock.
    last_notifier->GetContentSettings(GURL("http://random-hostname.com/"));
  }

  HostContentSettingsMap* last_notifier;
  ContentSettingsPattern last_pattern;
  bool last_update_all;
  bool last_update_all_types;
  int counter;
  ContentSettingsType last_type;

 private:
  NotificationRegistrar registrar_;
};
#endif  // CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_UNITTEST_H_

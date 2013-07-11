// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mock_settings_observer.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "url/gurl.h"

MockSettingsObserver::MockSettingsObserver() {
  registrar_.Add(this, chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED,
                 content::NotificationService::AllSources());
}

MockSettingsObserver::~MockSettingsObserver() {}

void MockSettingsObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  HostContentSettingsMap* map =
      content::Source<HostContentSettingsMap>(source).ptr();
  ContentSettingsDetails* settings_details =
      content::Details<ContentSettingsDetails>(details).ptr();
  OnContentSettingsChanged(map,
                           settings_details->type(),
                           settings_details->update_all_types(),
                           settings_details->primary_pattern(),
                           settings_details->secondary_pattern(),
                           settings_details->update_all());
  // This checks that calling a Get function from an observer doesn't
  // deadlock.
  GURL url("http://random-hostname.com/");
  map->GetContentSetting(url, url, CONTENT_SETTINGS_TYPE_IMAGES, std::string());
}

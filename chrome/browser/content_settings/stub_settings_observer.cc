// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/stub_settings_observer.h"

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "googleurl/src/gurl.h"

StubSettingsObserver::StubSettingsObserver()
    : last_notifier(NULL),
      counter(0) {
  registrar_.Add(this, NotificationType::CONTENT_SETTINGS_CHANGED,
                 NotificationService::AllSources());
}

StubSettingsObserver::~StubSettingsObserver() {}

void StubSettingsObserver::Observe(NotificationType type,
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

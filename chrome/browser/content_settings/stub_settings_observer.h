// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_STUB_SETTINGS_OBSERVER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_STUB_SETTINGS_OBSERVER_H_
#pragma once

#include "chrome/browser/content_settings/content_settings_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"

class HostContentSettingsMap;

class StubSettingsObserver : public NotificationObserver {
 public:
  StubSettingsObserver();
  virtual ~StubSettingsObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  HostContentSettingsMap* last_notifier;
  ContentSettingsPattern last_pattern;
  bool last_update_all;
  bool last_update_all_types;
  int counter;
  ContentSettingsType last_type;

 private:
  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_STUB_SETTINGS_OBSERVER_H_

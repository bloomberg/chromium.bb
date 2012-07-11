// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_MOCK_SETTINGS_OBSERVER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_MOCK_SETTINGS_OBSERVER_H_

#include "chrome/common/content_settings_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "testing/gmock/include/gmock/gmock.h"

class ContentSettingsPattern;
class HostContentSettingsMap;

class MockSettingsObserver : public content::NotificationObserver {
 public:
  MockSettingsObserver();
  virtual ~MockSettingsObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  MOCK_METHOD6(OnContentSettingsChanged,
               void(HostContentSettingsMap*,
                    ContentSettingsType,
                    bool,
                    const ContentSettingsPattern&,
                    const ContentSettingsPattern&,
                    bool));

 private:
  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_MOCK_SETTINGS_OBSERVER_H_

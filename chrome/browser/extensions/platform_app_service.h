// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_SERVICE_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {

// The PlatformAppService provides services for platform apps, such as
// keeping the browser process alive while they are running.
// TODO(benwells): Merge functionality of ShellWindowRegistry into this class.
class PlatformAppService : public ProfileKeyedService,
                           public content::NotificationObserver {
 public:
  explicit PlatformAppService(Profile* profile);
  virtual ~PlatformAppService();

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_SERVICE_H_

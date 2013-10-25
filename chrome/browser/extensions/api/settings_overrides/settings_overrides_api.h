// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_

#include "base/basictypes.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {

class SettingsOverridesAPI : public ProfileKeyedAPI,
                             public content::NotificationObserver {
 public:
  explicit SettingsOverridesAPI(Profile* profile);
  virtual ~SettingsOverridesAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<SettingsOverridesAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<SettingsOverridesAPI>;
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  // ProfileKeyedAPI implementation.
  static const char* service_name() { return "SettingsOverridesAPI"; }

  Profile* profile_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SettingsOverridesAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_

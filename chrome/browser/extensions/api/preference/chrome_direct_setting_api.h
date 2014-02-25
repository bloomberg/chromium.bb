// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_CHROME_DIRECT_SETTING_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_CHROME_DIRECT_SETTING_API_H__

#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {
namespace chromedirectsetting {

class ChromeDirectSettingAPI : public ProfileKeyedAPI,
                               public EventRouter::Observer {
 public:
  explicit ChromeDirectSettingAPI(content::BrowserContext* context);

  virtual ~ChromeDirectSettingAPI();

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<ChromeDirectSettingAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

  // Returns true if the preference is on the whitelist.
  bool IsPreferenceOnWhitelist(const std::string& pref_key);

  // Convenience method to get the ChromeDirectSettingAPI for a profile.
  static ChromeDirectSettingAPI* Get(content::BrowserContext* context);

 private:
  friend class ProfileKeyedAPIFactory<ChromeDirectSettingAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name();

  void OnPrefChanged(PrefService* pref_service, const std::string& pref_key);

  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = false;

  PrefChangeRegistrar registrar_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDirectSettingAPI);
};

}  // namespace chromedirectsetting
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_CHROME_DIRECT_SETTING_API_H__

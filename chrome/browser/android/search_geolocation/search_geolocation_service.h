// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_SEARCH_GEOLOCATION_SERVICE_H_
#define CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_SEARCH_GEOLOCATION_SERVICE_H_

#include "base/memory/singleton.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "url/origin.h"

namespace content{
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class HostContentSettingsMap;
class PrefService;
class Profile;
class TemplateURLService;

// Helper class to manage the DSE Geolocation setting. It keeps the setting
// valid by watching change to the CCTLD and DSE, and also provides logic for
// whether the setting should be used and it's current value.
// Glossary:
//     DSE: Default Search Engine
//     CCTLD: Country Code Top Level Domain (e.g. google.com.au)
class SearchGeolocationService
    : public TemplateURLServiceObserver,
      public KeyedService {
 public:
  // Factory implementation will not create a service in incognito.
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static SearchGeolocationService* GetForBrowserContext(
        content::BrowserContext* context);

    static Factory* GetInstance();
   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    bool ServiceIsCreatedWithBrowserContext() const override;
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* profile) const override;
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit SearchGeolocationService(Profile* profile);

  // Returns whether the DSE geolocation setting is applicable for geolocation
  // requests for the given top level origin.
  bool UseDSEGeolocationSetting(const url::Origin& requesting_origin);

  // Returns the DSE geolocation setting, after applying any updates needed to
  // make it valid.
  bool GetDSEGeolocationSetting();

  // Changes the DSE geolocation setting.
  void SetDSEGeolocationSetting(bool setting);

  // KeyedService:
  void Shutdown() override;

 private:
  struct PrefValue;

  ~SearchGeolocationService() override;

  // TemplateURLServiceObserver
  // When the DSE CCTLD changes (either by changing their DSE or by changing
  // their CCTLD, and their DSE supports geolocation:
  // * If the DSE CCTLD origin permission is BLOCK, but the DSE geolocation
  //   setting is on, change the DSE geolocation setting to off
  // * If the DSE CCTLD origin permission is ALLOW, but the DSE geolocation
  //   setting is off, reset the DSE CCTLD origin permission to ASK.
  // Also, if the previous DSE did not support geolocation, and the new one
  // does, and the geolocation setting is on, reset whether the DSE geolocation
  // disclosure has been shown.
  void OnTemplateURLServiceChanged() override;

  // Initialize the DSE geolocation setting if it hasn't already been
  // initialized. Also, if it hasn't been initialized, reset whether the DSE
  // geolocation disclosure has been shown to ensure user who may have seen it
  // on earlier versions (due to Finch experiments) see it again.
  void InitializeDSEGeolocationSettingIfNeeded();

  // Check that the DSE geolocation setting is valid with respect to the content
  // setting. The rules of vaidity are:
  // * If the content setting is BLOCK, the DSE geolocation setting must
  //   be false.
  // * If the content setting is ALLOW, the DSE geolocation setting must be
  //   true.
  // * If the content setting is ASK, the DSE geolocation setting can be true or
  //   false.
  // One way the setting could become invalid is if the feature enabling the
  // setting was disabled (via Finch or flags), content settings were changed,
  // and the feature enabled again. Or the enterprise policy settings could have
  // been updated in a way that makes the setting invalid.
  void EnsureDSEGeolocationSettingIsValid();

  // Returns true if the current DSE is Google (Google is the only search engine
  // to currently support DSE geolocation).
  bool IsGoogleSearchEngine();

  // Returns the origin of the current CCTLD of the default search engine. If
  // the default search engine is not Google, will return a unique Origin.
  url::Origin GetDSECCTLD();

  PrefValue GetDSEGeolocationPref();
  void SetDSEGeolocationPref(const PrefValue& pref);

  // Retrieve the geolocation content setting for the current DSE CCTLD.
  ContentSetting GetCurrentContentSetting();

  // Reset the geolocation content setting for the current DSE CCTLD back to the
  // default.
  void ResetContentSetting();

  // Returns whether the user can change the geolocation content setting for the
  // current DSE CCTLD.
  bool IsContentSettingUserSettable();

  // Whether the feature/experiment setup is enabling the consistent search
  // geolocation system.
  bool UseConsistentSearchGeolocation();

  Profile* profile_;
  PrefService* pref_service_;
  HostContentSettingsMap* host_content_settings_map_;
  TemplateURLService* template_url_service_;
};

#endif  // CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_SEARCH_GEOLOCATION_SERVICE_H_

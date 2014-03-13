// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/content_settings.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

class ContentSettingsPattern;
class CookieSettingsWrapper;
class GURL;
class PrefService;
class Profile;

// A frontend to the cookie settings of |HostContentSettingsMap|. Handles
// cookie-specific logic such as blocking third-party cookies. Written on the UI
// thread and read on any thread. One instance per profile.
class CookieSettings : public RefcountedBrowserContextKeyedService {
 public:
  CookieSettings(
      HostContentSettingsMap* host_content_settings_map,
      PrefService* prefs);

  // Returns the default content setting (CONTENT_SETTING_ALLOW,
  // CONTENT_SETTING_BLOCK, or CONTENT_SETTING_SESSION_ONLY) for cookies. If
  // |provider_id| is not NULL, the id of the provider which provided the
  // default setting is assigned to it.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultCookieSetting(std::string* provider_id) const;

  // Returns true if the page identified by (|url|, |first_party_url|) is
  // allowed to read cookies.
  //
  // This may be called on any thread.
  bool IsReadingCookieAllowed(const GURL& url,
                              const GURL& first_party_url) const;

  // Returns true if the page identified by (|url|, |first_party_url|) is
  // allowed to set cookies (permanent or session only).
  //
  // This may be called on any thread.
  bool IsSettingCookieAllowed(const GURL& url,
                              const GURL& first_party_url) const;

  // Returns true if the cookie set by a page identified by |url| should be
  // session only. Querying this only makes sense if |IsSettingCookieAllowed|
  // has returned true.
  //
  // This may be called on any thread.
  bool IsCookieSessionOnly(const GURL& url) const;

  // Returns all patterns with a non-default cookie setting, mapped to their
  // actual settings, in the precedence order of the setting rules. |settings|
  // must be a non-NULL outparam.
  //
  // This may be called on any thread.
  void GetCookieSettings(ContentSettingsForOneType* settings) const;

  // Sets the default content setting (CONTENT_SETTING_ALLOW,
  // CONTENT_SETTING_BLOCK, or CONTENT_SETTING_SESSION_ONLY) for cookies.
  //
  // This should only be called on the UI thread.
  void SetDefaultCookieSetting(ContentSetting setting);

  // Sets the cookie setting for the given patterns.
  //
  // This should only be called on the UI thread.
  void SetCookieSetting(const ContentSettingsPattern& primary_pattern,
                        const ContentSettingsPattern& secondary_pattern,
                        ContentSetting setting);

  // Resets the cookie setting for the given patterns.
  //
  // This should only be called on the UI thread.
  void ResetCookieSetting(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern);

  // Detaches the |CookieSettings| from all |Profile|-related objects like
  // |PrefService|. This methods needs to be called before destroying the
  // |Profile|. Afterwards, only const methods can be called.
  virtual void ShutdownOnUIThread() OVERRIDE;

  // A helper for applying third party cookie blocking rules.
  ContentSetting GetCookieSetting(
      const GURL& url,
      const GURL& first_party_url,
      bool setting_cookie,
      content_settings::SettingSource* source) const;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  class Factory : public RefcountedBrowserContextKeyedServiceFactory {
   public:
    // Returns the |CookieSettings| associated with the |profile|.
    //
    // This should only be called on the UI thread.
    static scoped_refptr<CookieSettings> GetForProfile(Profile* profile);

    static Factory* GetInstance();

   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // |BrowserContextKeyedBaseFactory| methods:
    virtual void RegisterProfilePrefs(
        user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
    virtual content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const OVERRIDE;
    virtual scoped_refptr<RefcountedBrowserContextKeyedService>
        BuildServiceInstanceFor(
            content::BrowserContext* context) const OVERRIDE;
  };

 private:
  virtual ~CookieSettings();

  void OnBlockThirdPartyCookiesChanged();

  // Returns true if the "block third party cookies" preference is set.
  //
  // This method may be called on any thread.
  bool ShouldBlockThirdPartyCookies() const;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  PrefChangeRegistrar pref_change_registrar_;

  // Used around accesses to |block_third_party_cookies_| to guarantee thread
  // safety.
  mutable base::Lock lock_;

  bool block_third_party_cookies_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_H_

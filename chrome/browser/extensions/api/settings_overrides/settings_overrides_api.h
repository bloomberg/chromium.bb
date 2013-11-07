// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TemplateURL;

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

  typedef std::set<scoped_refptr<const Extension> > PendingExtensions;

  // Wrappers around PreferenceAPI.
  void SetPref(const std::string& extension_id,
               const std::string& pref_key,
               base::Value* value);
  void UnsetPref(const std::string& extension_id,
                 const std::string& pref_key);
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  void OnTemplateURLsLoaded();

  void RegisterSearchProvider(const Extension* extension) const;
  // ProfileKeyedAPI implementation.
  static const char* service_name() { return "SettingsOverridesAPI"; }

  Profile* profile_;
  TemplateURLService* url_service_;

  // List of extensions waiting for the TemplateURLService to Load to
  // have search provider registered.
  PendingExtensions pending_extensions_;

  content::NotificationRegistrar registrar_;
  scoped_ptr<TemplateURLService::Subscription> template_url_sub_;

  DISALLOW_COPY_AND_ASSIGN(SettingsOverridesAPI);
};

template <>
void ProfileKeyedAPIFactory<SettingsOverridesAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_

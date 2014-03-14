// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_FACTORY_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

class PluginPrefs;
class Profile;

class PluginPrefsFactory : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<PluginPrefs> GetPrefsForProfile(Profile* profile);

  static PluginPrefsFactory* GetInstance();

 private:
  friend class PluginPrefs;
  friend struct DefaultSingletonTraits<PluginPrefsFactory>;

  // Helper method for PluginPrefs::GetForTestingProfile.
  static scoped_refptr<RefcountedBrowserContextKeyedService>
      CreateForTestingProfile(content::BrowserContext* profile);

  PluginPrefsFactory();
  virtual ~PluginPrefsFactory();

  // RefcountedBrowserContextKeyedServiceFactory methods:
  virtual scoped_refptr<RefcountedBrowserContextKeyedService>
      BuildServiceInstanceFor(content::BrowserContext* context) const OVERRIDE;

  // BrowserContextKeyedServiceFactory methods:
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_FACTORY_H_

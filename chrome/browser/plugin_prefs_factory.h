// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_PREFS_FACTORY_H_
#define CHROME_BROWSER_PLUGIN_PREFS_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class PluginPrefs;
class PrefService;
class Profile;
class ProfileKeyedService;

class PluginPrefsFactory : public RefcountedProfileKeyedServiceFactory {
 public:
  static scoped_refptr<PluginPrefs> GetPrefsForProfile(Profile* profile);

  static PluginPrefsFactory* GetInstance();

 private:
  friend class PluginPrefs;
  friend struct DefaultSingletonTraits<PluginPrefsFactory>;

  // Helper method for PluginPrefs::GetForTestingProfile.
  static scoped_refptr<RefcountedProfileKeyedService> CreateForTestingProfile(
      Profile* profile);

  PluginPrefsFactory();
  virtual ~PluginPrefsFactory();

  // RefcountedProfileKeyedServiceFactory methods:
  virtual scoped_refptr<RefcountedProfileKeyedService> BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  // ProfileKeyedServiceFactory methods:
  virtual void RegisterUserPrefs(PrefService* prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
};

#endif  // CHROME_BROWSER_PLUGIN_PREFS_FACTORY_H_

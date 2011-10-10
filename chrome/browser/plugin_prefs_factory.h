// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_PREFS_FACTORY_H_
#define CHROME_BROWSER_PLUGIN_PREFS_FACTORY_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PluginPrefs;
class PrefService;
class Profile;
class ProfileKeyedService;

// A wrapper around PluginPrefs to own the reference to thre real object.
//
// This should totally go away; we need a generic bridge between PKSF and
// scope_refptrs.
class PluginPrefsWrapper : public ProfileKeyedService {
 public:
  explicit PluginPrefsWrapper(scoped_refptr<PluginPrefs> plugin_prefs);
  virtual ~PluginPrefsWrapper();

  PluginPrefs* plugin_prefs() { return plugin_prefs_.get(); }

 private:
  // ProfileKeyedService methods:
  virtual void Shutdown() OVERRIDE;

  scoped_refptr<PluginPrefs> plugin_prefs_;
};

class PluginPrefsFactory : public ProfileKeyedServiceFactory {
 public:
  static PluginPrefsFactory* GetInstance();

  PluginPrefsWrapper* GetWrapperForProfile(Profile* profile);

  // Factory function for use with
  // ProfileKeyedServiceFactory::SetTestingFactory.
  static ProfileKeyedService* CreateWrapperForProfile(Profile* profile);

  // Some unit tests that deal with PluginPrefs don't run with a Profile. Let
  // them still register their preferences.
  void ForceRegisterPrefsForTest(PrefService* prefs);

 private:
  friend struct DefaultSingletonTraits<PluginPrefsFactory>;

  PluginPrefsFactory();
  virtual ~PluginPrefsFactory();

  // ProfileKeyedServiceFactory methods:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() OVERRIDE;
};

#endif  // CHROME_BROWSER_PLUGIN_PREFS_FACTORY_H_

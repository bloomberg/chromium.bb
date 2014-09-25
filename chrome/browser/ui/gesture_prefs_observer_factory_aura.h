// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GESTURE_PREFS_OBSERVER_FACTORY_AURA_H_
#define CHROME_BROWSER_UI_GESTURE_PREFS_OBSERVER_FACTORY_AURA_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PrefService;
class Profile;

// Create an observer per Profile that listens for gesture preferences updates.
class GesturePrefsObserverFactoryAura
    : public BrowserContextKeyedServiceFactory {
 public:
  static GesturePrefsObserverFactoryAura* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GesturePrefsObserverFactoryAura>;

  GesturePrefsObserverFactoryAura();
  virtual ~GesturePrefsObserverFactoryAura();

  void RegisterOverscrollPrefs(user_prefs::PrefRegistrySyncable* registry);

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GesturePrefsObserverFactoryAura);
};

#endif  // CHROME_BROWSER_UI_GESTURE_PREFS_OBSERVER_FACTORY_AURA_H_

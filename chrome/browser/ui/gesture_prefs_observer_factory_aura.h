// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GESTURE_PREFS_OBSERVER_FACTORY_AURA_H_
#define CHROME_BROWSER_UI_GESTURE_PREFS_OBSERVER_FACTORY_AURA_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PrefService;
class Profile;

// Create an observer per Profile that listens for gesture preferences updates.
class GesturePrefsObserverFactoryAura : public ProfileKeyedServiceFactory {
 public:
  static GesturePrefsObserverFactoryAura* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GesturePrefsObserverFactoryAura>;

  GesturePrefsObserverFactoryAura();
  virtual ~GesturePrefsObserverFactoryAura();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* prefs) OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GesturePrefsObserverFactoryAura);
};

#endif  // CHROME_BROWSER_UI_GESTURE_PREFS_OBSERVER_FACTORY_AURA_H_

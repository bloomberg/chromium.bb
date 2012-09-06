// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_CWS_INTENTS_REGISTRY_FACTORY_H_
#define CHROME_BROWSER_INTENTS_CWS_INTENTS_REGISTRY_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class CWSIntentsRegistry;
class Profile;

// Singleton that owns all CWSIntentsRegistry objects and associates each with
// their respective profile. Listens for the profile's destruction notification
// and cleans up the associated CWSIntentsRegistry.
class CWSIntentsRegistryFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns a weak pointer to the WebIntentsRegistry that provides intent
  // registration for |profile|.
  static CWSIntentsRegistry* GetForProfile(Profile* profile);

  // Returns the singleton instance of the WebIntentsRegistryFactory.
  static CWSIntentsRegistryFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<CWSIntentsRegistryFactory>;

  CWSIntentsRegistryFactory();
  virtual ~CWSIntentsRegistryFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CWSIntentsRegistryFactory);
};

#endif  // CHROME_BROWSER_INTENTS_CWS_INTENTS_REGISTRY_FACTORY_H_

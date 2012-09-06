// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_FACTORY_H_
#define CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class WebIntentsRegistry;
template <typename T> struct DefaultSingletonTraits;

// Singleton that owns all WebIntentsRegistrys and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated WebIntentsRegistry.
class WebIntentsRegistryFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the WebIntentsRegistry that provides intent registration for
  // |profile|. Ownership stays with this factory object.
  static WebIntentsRegistry* GetForProfile(Profile* profile);

  // Returns the singleton instance of the WebIntentsRegistryFactory.
  static WebIntentsRegistryFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<WebIntentsRegistryFactory>;

  WebIntentsRegistryFactory();
  virtual ~WebIntentsRegistryFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsRegistryFactory);
};

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENTS_REGISTRY_FACTORY_H_

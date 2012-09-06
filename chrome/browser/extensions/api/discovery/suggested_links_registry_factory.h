// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINKS_REGISTRY_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINKS_REGISTRY_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class SuggestedLinksRegistry;

// Singleton that associate SuggestedLinksRegistry objects with Profiles.
class SuggestedLinksRegistryFactory : public ProfileKeyedServiceFactory {
 public:
  static SuggestedLinksRegistry* GetForProfile(Profile* profile);

  static SuggestedLinksRegistryFactory* GetInstance();

  // Overridden from ProfileKeyedBaseFactory:
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<SuggestedLinksRegistryFactory>;

  SuggestedLinksRegistryFactory();
  virtual ~SuggestedLinksRegistryFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SuggestedLinksRegistryFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINKS_REGISTRY_FACTORY_H_

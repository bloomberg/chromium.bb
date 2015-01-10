// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FAVICON_FAVICON_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"

template <typename T> struct DefaultSingletonTraits;

class FaviconService;
class Profile;

// Singleton that owns all FaviconService and associates them with
// Profiles.
class FaviconServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // |access| defines what the caller plans to do with the service. See
  // the ServiceAccessType definition in profile.h.
  static FaviconService* GetForProfile(Profile* profile, ServiceAccessType sat);

  static FaviconServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<FaviconServiceFactory>;

  FaviconServiceFactory();
  ~FaviconServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(FaviconServiceFactory);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_SERVICE_FACTORY_H_

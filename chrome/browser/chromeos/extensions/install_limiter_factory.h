// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {

class InstallLimiter;

// Singleton that owns all InstallLimiter and associates them with profiles.
class InstallLimiterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static InstallLimiter* GetForProfile(Profile* profile);

  static InstallLimiterFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<InstallLimiterFactory>;

  InstallLimiterFactory();
  ~InstallLimiterFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(InstallLimiterFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_

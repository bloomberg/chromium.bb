// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOGO_SERVICE_H_
#define CHROME_BROWSER_ANDROID_LOGO_SERVICE_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_provider_logos/logo_tracker.h"

class Profile;

// Provides the logo for a profile's default search provider.
//
// Example usage:
//   LogoService* logo_service = LogoServiceFactory::GetForProfile(profile);
//   logo_service->GetLogo(...);
//
class LogoService : public KeyedService {
 public:
  explicit LogoService(Profile* profile);
  ~LogoService() override;

  // Gets the logo for the default search provider and notifies |observer|
  // with the results.
  void GetLogo(search_provider_logos::LogoObserver* observer);

 private:
  Profile* profile_;
  std::unique_ptr<search_provider_logos::LogoTracker> logo_tracker_;

  DISALLOW_COPY_AND_ASSIGN(LogoService);
};

// Singleton that owns all LogoServices and associates them with Profiles.
class LogoServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LogoService* GetForProfile(Profile* profile);

  static LogoServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<LogoServiceFactory>;

  LogoServiceFactory();
  ~LogoServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ANDROID_LOGO_SERVICE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_ANDROID_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_METRICS_ANDROID_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class AndroidProfileSessionDurationsService;
class Profile;

class AndroidProfileSessionDurationsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // If there is an active user profile, then it returns the service associated
  // with that profile (it creates the service if it does not exist already) .
  static AndroidProfileSessionDurationsService* GetForActiveUserProfile();

  // Creates the service if it doesn't exist already for the given
  // BrowserContext.
  static AndroidProfileSessionDurationsService* GetForProfile(Profile* profile);

  static AndroidProfileSessionDurationsServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      AndroidProfileSessionDurationsServiceFactory>;

  AndroidProfileSessionDurationsServiceFactory();
  ~AndroidProfileSessionDurationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(AndroidProfileSessionDurationsServiceFactory);
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_ANDROID_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

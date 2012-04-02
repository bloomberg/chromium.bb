// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_FACTORY_H_
#pragma once

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class ChromeToMobileService;

class ChromeToMobileServiceFactory
    : public RefcountedProfileKeyedServiceFactory {
 public:
  // Get the singleton ChromeToMobileServiceFactory instance.
  static ChromeToMobileServiceFactory* GetInstance();

  // Get |profile|'s ChromeToMobileService, creating one if needed.
  static scoped_refptr<ChromeToMobileService> GetForProfile(Profile* profile);

 protected:
  // RefcountedProfileKeyedServiceFactory overrides:
  virtual scoped_refptr<RefcountedProfileKeyedService> BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ChromeToMobileServiceFactory>;

  explicit ChromeToMobileServiceFactory();
  virtual ~ChromeToMobileServiceFactory();

  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileServiceFactory);
};

#endif  // CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_FACTORY_H_

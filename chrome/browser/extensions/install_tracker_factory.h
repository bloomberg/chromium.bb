// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

template <typename T> struct DefaultSingletonTraits;

class Profile;

namespace extensions {

class InstallTracker;

class InstallTrackerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static InstallTracker* GetForProfile(Profile* profile);
  static InstallTrackerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<InstallTrackerFactory>;

  InstallTrackerFactory();
  virtual ~InstallTrackerFactory();

  // BrowserContextKeyedServiceFactory overrides:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InstallTrackerFactory);
};

}  // namespace extensions;

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_FACTORY_H_
#define CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class SyncGlobalError;
class Profile;

// Singleton that owns all SyncGlobalErrors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SyncGlobalError.
class SyncGlobalErrorFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of SyncGlobalError associated with this profile,
  // creating one if none exists. In Ash, this will return NULL.
  static SyncGlobalError* GetForProfile(Profile* profile);

  // Returns an instance of the SyncGlobalErrorFactory singleton.
  static SyncGlobalErrorFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SyncGlobalErrorFactory>;

  SyncGlobalErrorFactory();
  virtual ~SyncGlobalErrorFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SyncGlobalErrorFactory);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_GLOBAL_ERROR_FACTORY_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class HistoryService;

// Singleton that owns all HistoryService and associates them with
// Profiles.
class HistoryServiceFactory
    : public RefcountedProfileKeyedServiceFactory {
 public:
  static scoped_refptr<HistoryService> GetForProfile(
      Profile* profile, Profile::ServiceAccessType sat);

  static scoped_refptr<HistoryService> GetForProfileIfExists(
      Profile* profile, Profile::ServiceAccessType sat);

  static scoped_refptr<HistoryService> GetForProfileWithoutCreating(
      Profile* profile);

  static HistoryServiceFactory* GetInstance();

  // In the testing profile, we often clear the history before making a new
  // one. This takes care of that work. It should only be used in tests.
  // Note: This does not do any cleanup; it only destroys the service. The
  // calling test is expected to do the cleanup before calling this function.
  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<HistoryServiceFactory>;

  HistoryServiceFactory();
  virtual ~HistoryServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual scoped_refptr<RefcountedProfileKeyedService> BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_

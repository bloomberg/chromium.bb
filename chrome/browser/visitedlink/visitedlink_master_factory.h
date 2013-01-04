// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISITEDLINK_VISITEDLINK_MASTER_FACTORY_H_
#define CHROME_BROWSER_VISITEDLINK_VISITEDLINK_MASTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

template <typename T> struct DefaultSingletonTraits;

class VisitedLinkMaster;

// Singleton that owns all VisitedLinkMaster and associates them with
// Profiles.
class VisitedLinkMasterFactory : public ProfileKeyedServiceFactory {
 public:
  static VisitedLinkMaster* GetForProfile(Profile* profile);

  static VisitedLinkMasterFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<VisitedLinkMasterFactory>;

  VisitedLinkMasterFactory();
  virtual ~VisitedLinkMasterFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(VisitedLinkMasterFactory);
};

#endif  // CHROME_BROWSER_VISITEDLINK_VISITEDLINK_MASTER_FACTORY_H_

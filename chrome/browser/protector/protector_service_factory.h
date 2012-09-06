// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"

namespace protector {

class ProtectorService;

class ProtectorServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the ProtectorService instance for |profile|.
  static ProtectorService* GetForProfile(Profile* profile);

  static ProtectorServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ProtectorServiceFactory>;

  ProtectorServiceFactory();
  virtual ~ProtectorServiceFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* user_prefs) OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ProtectorServiceFactory);
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_FACTORY_H_

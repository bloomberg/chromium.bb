// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_FACTORY_H__

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {
class ProcessesAPI;

class ProcessesAPIFactory : public ProfileKeyedServiceFactory {
 public:
  static ProcessesAPI* GetForProfile(Profile* profile);

  static ProcessesAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ProcessesAPIFactory>;

  ProcessesAPIFactory();
  virtual ~ProcessesAPIFactory();

  // ProfileKeyedBaseFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_FACTORY_H__

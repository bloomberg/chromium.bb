// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ExtensionManagementAPI;

class ExtensionManagementAPIFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionManagementAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionManagementAPIFactory>;

  ExtensionManagementAPIFactory();
  virtual ~ExtensionManagementAPIFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_FACTORY_H_

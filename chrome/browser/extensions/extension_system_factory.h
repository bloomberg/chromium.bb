// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ProfileKeyedService;

namespace extensions {
class ExtensionSystem;

// ProfileKeyedServiceFactory for ExtensionSystemImpl::Shared.
// Should not be used except by ExtensionSystem(Factory).
class ExtensionSystemSharedFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionSystemImpl::Shared* GetForProfile(
      Profile* profile);

  static ExtensionSystemSharedFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSystemSharedFactory>;

  ExtensionSystemSharedFactory();
  virtual ~ExtensionSystemSharedFactory();

  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
};

// ProfileKeyedServiceFactory for ExtensionSystem.
class ExtensionSystemFactory : public ProfileKeyedServiceFactory {
 public:
  // ProfileKeyedServiceFactory implementation:
  static ExtensionSystem* GetForProfile(Profile* profile);

  static ExtensionSystemFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSystemFactory>;

  ExtensionSystemFactory();
  virtual ~ExtensionSystemFactory();

  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceHasOwnInstanceInIncognito() const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_

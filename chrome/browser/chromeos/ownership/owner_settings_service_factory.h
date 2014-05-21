// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_FACTORY_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace chromeos {

class OwnerSettingsService;

class OwnerSettingsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OwnerSettingsService* GetForProfile(Profile* profile);

  static OwnerSettingsServiceFactory* GetInstance();

  // Sets name of the user supposed to be an owner. If profile
  // for |username| is ready, request to reload owner key will be
  // sent. Otherwise, owner key will be reloaded as soon as profile will
  // be ready.
  void SetUsername(const std::string& username);

  // Returns the name of the user supposed to be an owner.
  std::string GetUsername() const;

 private:
  friend struct DefaultSingletonTraits<OwnerSettingsServiceFactory>;

  OwnerSettingsServiceFactory();
  virtual ~OwnerSettingsServiceFactory();

  static KeyedService* BuildInstanceFor(content::BrowserContext* context);

  // BrowserContextKeyedBaseFactory overrides:
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;

  // BrowserContextKeyedServiceFactory implementation:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const OVERRIDE;

  // Name of the user supposed to be an owner.
  std::string username_;

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_FACTORY_H_

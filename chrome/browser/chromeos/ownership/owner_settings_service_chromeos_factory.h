// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace ownership {
class OwnerKeyUtil;
}

namespace chromeos {

class OwnerSettingsServiceChromeOS;

class OwnerSettingsServiceChromeOSFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OwnerSettingsServiceChromeOS* GetForProfile(Profile* profile);

  static OwnerSettingsServiceChromeOSFactory* GetInstance();

  scoped_refptr<ownership::OwnerKeyUtil> GetOwnerKeyUtil();

  void SetOwnerKeyUtilForTesting(
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

 private:
  friend struct DefaultSingletonTraits<OwnerSettingsServiceChromeOSFactory>;

  OwnerSettingsServiceChromeOSFactory();
  virtual ~OwnerSettingsServiceChromeOSFactory();

  static KeyedService* BuildInstanceFor(content::BrowserContext* context);

  // BrowserContextKeyedBaseFactory overrides:
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;

  // BrowserContextKeyedServiceFactory implementation:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const OVERRIDE;

  scoped_refptr<ownership::OwnerKeyUtil> owner_key_util_;

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOSFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_FACTORY_H_

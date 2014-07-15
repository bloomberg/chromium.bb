// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_

template<typename T> struct DefaultSingletonTraits;

namespace content {
class BrowserContext;
}  // namespace content

namespace file_manager {

class VolumeManager;

// Factory to create VolumeManager.
class VolumeManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns VolumeManager instance.
  static VolumeManager* Get(content::BrowserContext* context);

  static VolumeManagerFactory* GetInstance();

 protected:
  // BrowserContextKeyedBaseFactory overrides:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  // BrowserContextKeyedServiceFactory overrides:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<VolumeManagerFactory>;

  VolumeManagerFactory();
  virtual ~VolumeManagerFactory();

  DISALLOW_COPY_AND_ASSIGN(VolumeManagerFactory);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_

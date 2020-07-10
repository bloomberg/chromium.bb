// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace plugin_vm {

class PluginVmImageManager;

class PluginVmImageManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PluginVmImageManager* GetForProfile(Profile* profile);
  static PluginVmImageManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PluginVmImageManagerFactory>;

  PluginVmImageManagerFactory();
  ~PluginVmImageManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManagerFactory);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_FACTORY_H_

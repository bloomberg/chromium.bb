// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {

class ExtensionToolbarModel;

class ExtensionToolbarModelFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionToolbarModel* GetForProfile(Profile* profile);

  static ExtensionToolbarModelFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionToolbarModelFactory>;

  ExtensionToolbarModelFactory();
  ~ExtensionToolbarModelFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_FACTORY_H_

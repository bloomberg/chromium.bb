// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREFS_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREFS_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class ExtensionPrefs;

class ExtensionPrefsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionPrefs* GetForBrowserContext(content::BrowserContext* context);

  static ExtensionPrefsFactory* GetInstance();

  void SetInstanceForTesting(
      content::BrowserContext* context, ExtensionPrefs* prefs);

 private:
  friend struct DefaultSingletonTraits<ExtensionPrefsFactory>;

  ExtensionPrefsFactory();
  virtual ~ExtensionPrefsFactory();

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREFS_FACTORY_H_

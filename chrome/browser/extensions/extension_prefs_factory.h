// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

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

  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_FACTORY_H_

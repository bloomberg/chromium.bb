// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_VALUE_MAP_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_VALUE_MAP_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class ExtensionPrefValueMap;
class Profile;

// The usual factory boilerplate for ExtensionPrefValueMap.
class ExtensionPrefValueMapFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionPrefValueMap* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionPrefValueMapFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionPrefValueMapFactory>;

  ExtensionPrefValueMapFactory();
  virtual ~ExtensionPrefValueMapFactory();

  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_VALUE_MAP_FACTORY_H_

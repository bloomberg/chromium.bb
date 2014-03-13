// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ExtensionPrefValueMap;

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

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_FACTORY_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_

#include "base/macros.h"

class KeyedServiceBaseFactory;
enum class ServiceAccessType;

namespace ios {

class ChromeBrowserState;
class KeyedServiceProvider;

// Registers and returns the global KeyedService provider.
void SetKeyedServiceProvider(KeyedServiceProvider* provider);
KeyedServiceProvider* GetKeyedServiceProvider();

// A class that provides access to KeyedService that do not have a pure iOS
// implementation yet.
class KeyedServiceProvider {
 public:
  KeyedServiceProvider();
  virtual ~KeyedServiceProvider();

  // Ensures that all KeyedService factories are instantiated. Must be called
  // before any BrowserState instance is created so that dependencies are
  // correct.
  void AssertKeyedFactoriesBuilt();

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyedServiceProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_

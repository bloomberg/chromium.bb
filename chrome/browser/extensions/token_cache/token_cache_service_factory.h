// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TOKEN_CACHE_TOKEN_CACHE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_TOKEN_CACHE_TOKEN_CACHE_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {
class TokenCacheService;
}  // namespace extensions

class TokenCacheServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static extensions::TokenCacheService* GetForProfile(Profile* profile);
  static TokenCacheServiceFactory* GetInstance();

 private:
  TokenCacheServiceFactory();
  virtual ~TokenCacheServiceFactory();

  friend struct DefaultSingletonTraits<TokenCacheServiceFactory>;

  // Inherited from ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

   DISALLOW_COPY_AND_ASSIGN(TokenCacheServiceFactory);
};

#endif  // CHROME_BROWSER_EXTENSIONS_TOKEN_CACHE_TOKEN_CACHE_SERVICE_FACTORY_H_

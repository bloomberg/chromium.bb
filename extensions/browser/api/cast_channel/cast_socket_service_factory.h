// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_SERVICE_FACTORY_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_SERVICE_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {
namespace api {
namespace cast_channel {

class CastSocketService;

// TODO(crbug.com/725717): CastSocket created by one profile (browser context)
// could be shared with other profiles.
class CastSocketServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Caller needs to make sure that it passes in the same |context| instance to
  // this function for both normal profile and incognito profile.
  static CastSocketService* GetForBrowserContext(
      content::BrowserContext* context);

  static CastSocketServiceFactory* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<CastSocketServiceFactory>;

  CastSocketServiceFactory();
  ~CastSocketServiceFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CastSocketServiceFactory);
};

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_SERVICE_FACTORY_H_

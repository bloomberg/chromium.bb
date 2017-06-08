// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_SOCKET_SERVICE_FACTORY_H_
#define COMPONENTS_CAST_CHANNEL_CAST_SOCKET_SERVICE_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

namespace cast_channel {

class CastSocketService;

// TODO(crbug.com/725717): CastSocket created by one profile (browser context)
// could be shared with other profiles.
class CastSocketServiceFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  // Caller needs to make sure that it passes in the same |context| instance to
  // this function for both normal profile and incognito profile.
  static scoped_refptr<CastSocketService> GetForBrowserContext(
      content::BrowserContext* context);

  static CastSocketServiceFactory* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<CastSocketServiceFactory>;

  CastSocketServiceFactory();
  ~CastSocketServiceFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CastSocketServiceFactory);
};

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_SOCKET_SERVICE_FACTORY_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace media_router {

class MediaRouterImpl;

// A factory that returns a MediaRouter object for a given BrowserContext,
// creating one lazily if needed.
class MediaRouterImplFactory : public BrowserContextKeyedServiceFactory {
 public:
  static MediaRouterImpl* GetMediaRouterForBrowserContext(
      content::BrowserContext* context);
  static MediaRouterImplFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<MediaRouterImplFactory>;

  MediaRouterImplFactory();
  ~MediaRouterImplFactory() override;

  // BrowserContextKeyedBaseFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterImplFactory);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_FACTORY_H_

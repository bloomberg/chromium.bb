// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace media_router {

class MediaRouter;

// A factory that lazily returns a MediaRouter implementation for a given
// BrowserContext.
class MediaRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static MediaRouter* GetApiForBrowserContext(content::BrowserContext* context);

 private:
  friend struct base::DefaultLazyInstanceTraits<MediaRouterFactory>;

  MediaRouterFactory();
  ~MediaRouterFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterFactory);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FACTORY_H_

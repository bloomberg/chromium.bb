// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_OFFSCREEN_PRESENTATION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_OFFSCREEN_PRESENTATION_MANAGER_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace media_router {

class OffscreenPresentationManager;

// OffscreenPresentation manager is shared between a Profile and
// its associated incognito Profiles.
class OffscreenPresentationManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // If |web_contents| is normal profile, use it as browser context;
  // If |web_contents| is incognito profile, |GetBrowserContextToUse| will
  // redirect incognito profile to original profile, and use original one as
  // browser context.
  static OffscreenPresentationManager* GetOrCreateForWebContents(
      content::WebContents* web_contents);
  static OffscreenPresentationManager* GetOrCreateForBrowserContext(
      content::BrowserContext* context);

  // For test use only.
  static OffscreenPresentationManagerFactory* GetInstanceForTest();

 private:
  friend struct base::DefaultLazyInstanceTraits<
      OffscreenPresentationManagerFactory>;

  OffscreenPresentationManagerFactory();
  ~OffscreenPresentationManagerFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(OffscreenPresentationManagerFactory);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_OFFSCREEN_PRESENTATION_MANAGER_FACTORY_H_

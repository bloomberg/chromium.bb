// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_
#define EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace extensions {

// Informs renderers about extensions-related data (loaded extensions, available
// functions, etc.) when they start. Sends this information to both extension
// and non-extension renderers, as the non-extension renderers may have content
// scripts. Lives on the UI thread. Shared between incognito and non-incognito
// browser contexts.
class RendererStartupHelper : public KeyedService,
                              public content::NotificationObserver {
 public:
  // This class sends messages to all renderers started for |browser_context|.
  explicit RendererStartupHelper(content::BrowserContext* browser_context);
  virtual ~RendererStartupHelper();

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  content::BrowserContext* browser_context_;  // Not owned.

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RendererStartupHelper);
};

// Factory for RendererStartupHelpers. Declared here because this header is
// rarely included and it's probably cheaper to put it here than to make the
// compiler generate another object file.
class RendererStartupHelperFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RendererStartupHelper* GetForBrowserContext(
      content::BrowserContext* context);
  static RendererStartupHelperFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<RendererStartupHelperFactory>;

  RendererStartupHelperFactory();
  virtual ~RendererStartupHelperFactory();

  // BrowserContextKeyedServiceFactory implementation:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(RendererStartupHelperFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_ORIGINS_SEEN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TAB_CONTENTS_ORIGINS_SEEN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace navigation_metrics {
class OriginsSeenService;
}

class OriginsSeenServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static navigation_metrics::OriginsSeenService* GetForBrowserContext(
      content::BrowserContext* context);

  static OriginsSeenServiceFactory* GetInstance();

  // Used to create instances for testing.
  static KeyedService* BuildInstanceFor(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<OriginsSeenServiceFactory>;

  OriginsSeenServiceFactory();
  ~OriginsSeenServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_ORIGINS_SEEN_SERVICE_FACTORY_H_

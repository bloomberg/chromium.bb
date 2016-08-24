// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CLIENT_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CLIENT_CONTEXT_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace blimp {
namespace client {
class BlimpClientContext;
}  // namespace client
}  // namespace blimp

namespace content {
class BrowserContext;
}  // namespace content

// BlimpClientContextFactory is the main client class for interactive with
// blimp.
class BlimpClientContextFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of BlimpClientContextFactory.
  static BlimpClientContextFactory* GetInstance();

  // Returns the BlimpClientContext associated with |context| or creates a new
  // one if it doesn't exist.
  static blimp::client::BlimpClientContext* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<BlimpClientContextFactory>;

  BlimpClientContextFactory();
  ~BlimpClientContextFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextFactory);
};

#endif  // CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CLIENT_CONTEXT_FACTORY_H_

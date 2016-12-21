// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_LOADER_OFFLINER_FACTORY_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_LOADER_OFFLINER_FACTORY_H_

#include "base/macros.h"
#include "components/offline_pages/core/background/offliner_factory.h"

namespace content {
class BrowserContext;
} // namespace content

namespace offline_pages {

class OfflinerPolicy;
class Offliner;
class BackgroundLoaderOffliner;

class BackgroundLoaderOfflinerFactory : public OfflinerFactory {
 public:
  explicit BackgroundLoaderOfflinerFactory(content::BrowserContext* context);
  ~BackgroundLoaderOfflinerFactory() override;

  Offliner* GetOffliner(const OfflinerPolicy* policy) override;

 private:
  // Offliner owned by the factory.
  BackgroundLoaderOffliner* offliner_;

  // Unowned pointer, expected to stay valid for the lifetime of the factory.
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundLoaderOfflinerFactory);
};

} // namespace offline_pages

#endif // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_LOADER_OFFLINER_FACTORY_H_

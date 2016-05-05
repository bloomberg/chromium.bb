// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_FACTORY_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_FACTORY_H_

#include "base/macros.h"
#include "components/offline_pages/background/offliner_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace offline_pages {

class OfflinerPolicy;
class Offliner;
class PrerenderingOffliner;

// A factory to create one unique PrerenderingOffliner.  Since the
// PrerenderingOffliner is somewhat heavyweight and can use lots of memory, we
// normally don't want more than one, and we can reuse the offliner for many
// offlining sessions.
class PrerenderingOfflinerFactory : public OfflinerFactory {
 public:
  explicit PrerenderingOfflinerFactory(content::BrowserContext* context);
  ~PrerenderingOfflinerFactory() override;

  Offliner* GetOffliner(const OfflinerPolicy* policy) override;

 private:
  // The offliner is owned by the factory, since it may be resued.
  PrerenderingOffliner* offliner_;

  // This is an unowned pointer, expected to stay valid for the lifetime of the
  // factory.
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingOfflinerFactory);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_FACTORY_H_

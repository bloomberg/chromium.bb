// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENHANCED_BOOKMARKS_CHROME_BOOKMARK_SERVER_CLUSTER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENHANCED_BOOKMARKS_CHROME_BOOKMARK_SERVER_CLUSTER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;

namespace enhanced_bookmarks {

class ChromeBookmarkServerClusterService;

// A factory to create one unique ChromeBookmarkServerClusterService.
class ChromeBookmarkServerClusterServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeBookmarkServerClusterServiceFactory* GetInstance();
  static ChromeBookmarkServerClusterService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct DefaultSingletonTraits<
      ChromeBookmarkServerClusterServiceFactory>;

  ChromeBookmarkServerClusterServiceFactory();
  ~ChromeBookmarkServerClusterServiceFactory() override {}

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ChromeBookmarkServerClusterServiceFactory);
};

}  // namespace enhanced_bookmarks

#endif  // CHROME_BROWSER_ENHANCED_BOOKMARKS_CHROME_BOOKMARK_SERVER_CLUSTER_SERVICE_FACTORY_H_

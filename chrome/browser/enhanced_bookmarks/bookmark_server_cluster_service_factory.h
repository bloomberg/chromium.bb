// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_CLUSTER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_CLUSTER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace enhanced_bookmarks {

class BookmarkServerClusterService;

// A factory to create one unique BookmarkServerClusterService.
class BookmarkServerClusterServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static BookmarkServerClusterServiceFactory* GetInstance();
  static BookmarkServerClusterService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<
      BookmarkServerClusterServiceFactory>;

  BookmarkServerClusterServiceFactory();
  ~BookmarkServerClusterServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BookmarkServerClusterServiceFactory);
};

}  // namespace enhanced_bookmarks

#endif  // CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_CLUSTER_SERVICE_FACTORY_H_

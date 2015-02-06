// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_BOOKMARK_IMAGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_BOOKMARK_IMAGE_SERVICE_FACTORY_H_

#include "components/enhanced_bookmarks/bookmark_image_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace enhanced_bookmarks {

class BookmarkImageServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BookmarkImageServiceFactory* GetInstance();
  static BookmarkImageService* GetForBrowserContext(
      content::BrowserContext* context);

  ~BookmarkImageServiceFactory() override;

 private:
  friend struct DefaultSingletonTraits<BookmarkImageServiceFactory>;

  BookmarkImageServiceFactory();

  BookmarkImageService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace enhanced_bookmarks

#endif  // CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_BOOKMARK_IMAGE_SERVICE_FACTORY_H_

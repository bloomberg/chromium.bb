// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;

namespace ios {
class ChromeBrowserState;
}

class BookmarkImageServiceIOS;

// Singleton that owns all BookmarkImageServices and associates them with
// ChromeBrowserStates.
class BookmarkImageServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static BookmarkImageServiceIOS* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static BookmarkImageServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<BookmarkImageServiceFactory>;

  BookmarkImageServiceFactory();
  ~BookmarkImageServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BookmarkImageServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_FACTORY_H_

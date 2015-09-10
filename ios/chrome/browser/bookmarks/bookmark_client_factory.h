// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_CLIENT_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_CLIENT_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespase base

class BookmarkClientImpl;

namespace ios {
class ChromeBrowserState;
}

// Singleton that owns all BookmarkClientImpls and associates them with
// ios::ChromeBrowserState.
class BookmarkClientFactory : public BrowserStateKeyedServiceFactory {
 public:
  static BookmarkClientImpl* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static BookmarkClientFactory* GetInstance();
  static TestingFactoryFunction GetDefaultFactory();

 private:
  friend struct base::DefaultSingletonTraits<BookmarkClientFactory>;

  BookmarkClientFactory();
  ~BookmarkClientFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(BookmarkClientFactory);
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_CLIENT_FACTORY_H_

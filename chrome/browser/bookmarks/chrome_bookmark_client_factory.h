// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_FACTORY_H_
#define CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class ChromeBookmarkClient;
class Profile;

// Singleton that owns all ChromeBookmarkClients and associates them with
// Profiles.
class ChromeBookmarkClientFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeBookmarkClient* GetForProfile(Profile* profile);
  static ChromeBookmarkClientFactory* GetInstance();
  static TestingFactoryFunction GetDefaultFactory();

 private:
  friend struct base::DefaultSingletonTraits<ChromeBookmarkClientFactory>;

  ChromeBookmarkClientFactory();
  ~ChromeBookmarkClientFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(ChromeBookmarkClientFactory);
};

#endif  // CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_FACTORY_H_

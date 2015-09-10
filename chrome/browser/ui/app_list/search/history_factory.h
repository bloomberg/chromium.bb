// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace app_list {

class History;

// Singleton that owns all History and associates them with profiles;
class HistoryFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of HistoryFactory.
  static HistoryFactory* GetInstance();

  // Returns History associated with |context|.
  static History* GetForBrowserContext(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<HistoryFactory>;

  HistoryFactory();
  ~HistoryFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(HistoryFactory);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_FACTORY_H_

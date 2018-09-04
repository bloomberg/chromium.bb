// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_QUERY_IN_OMNIBOX_FACTORY_H_
#define CHROME_BROWSER_UI_OMNIBOX_QUERY_IN_OMNIBOX_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class QueryInOmnibox;
class Profile;

// Singleton that owns all QueryInOmnibox instances and associates them with
// Profiles.
class QueryInOmniboxFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the QueryInOmnibox for |profile|.
  static QueryInOmnibox* GetForProfile(Profile* profile);

  static QueryInOmniboxFactory* GetInstance();

  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<QueryInOmniboxFactory>;

  QueryInOmniboxFactory();
  ~QueryInOmniboxFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(QueryInOmniboxFactory);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_QUERY_IN_OMNIBOX_FACTORY_H_

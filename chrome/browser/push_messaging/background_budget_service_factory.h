// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BackgroundBudgetService;

class BackgroundBudgetServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static BackgroundBudgetService* GetForProfile(
      content::BrowserContext* context);
  static BackgroundBudgetServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<BackgroundBudgetServiceFactory>;

  BackgroundBudgetServiceFactory();
  ~BackgroundBudgetServiceFactory() override {}

  // BrowserContextKeyedServiceFactory interface.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BackgroundBudgetServiceFactory);
};

#endif  //  CHROME_BROWSER_PUSH_MESSAGING_BACKGROUND_BUDGET_SERVICE_FACTORY_H_

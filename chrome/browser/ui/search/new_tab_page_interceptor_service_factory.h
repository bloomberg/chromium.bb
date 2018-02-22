// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_INTERCEPTOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_INTERCEPTOR_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class NewTabPageInterceptorService;
class Profile;

namespace content {
class BrowserContext;
}

// Owns and creates NewTabPageInterceptorService instances.
class NewTabPageInterceptorServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NewTabPageInterceptorService* GetForProfile(Profile* profile);
  static NewTabPageInterceptorServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      NewTabPageInterceptorServiceFactory>;

  NewTabPageInterceptorServiceFactory();
  ~NewTabPageInterceptorServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(NewTabPageInterceptorServiceFactory);
};

#endif  // CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_INTERCEPTOR_SERVICE_FACTORY_H_

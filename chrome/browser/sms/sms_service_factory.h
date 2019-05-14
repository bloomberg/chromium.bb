// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMS_SMS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SMS_SMS_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class SmsKeyedService;

// Factory to get or create an instance of SmsService from
// a Profile.
class SmsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SmsKeyedService* GetForProfile(content::BrowserContext* profile);
  static SmsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SmsServiceFactory>;

  SmsServiceFactory();
  ~SmsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(SmsServiceFactory);
};

#endif  // CHROME_BROWSER_SMS_SMS_SERVICE_FACTORY_H_

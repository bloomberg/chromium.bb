// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class CertificateReportingService;

class CertificateReportingServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of CertificateReportingServiceFactory.
  static CertificateReportingServiceFactory* GetInstance();

  // Returns the reporting service associated with |context|.
  static CertificateReportingService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<
      CertificateReportingServiceFactory>;

  CertificateReportingServiceFactory();
  ~CertificateReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CertificateReportingServiceFactory);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
CertificateReportingServiceFactory*
CertificateReportingServiceFactory::GetInstance() {
  return base::Singleton<CertificateReportingServiceFactory>::get();
}

// static
CertificateReportingService*
CertificateReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CertificateReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

CertificateReportingServiceFactory::CertificateReportingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "cert_reporting::Factory",
          BrowserContextDependencyManager::GetInstance()) {}

CertificateReportingServiceFactory::~CertificateReportingServiceFactory() {}

KeyedService* CertificateReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new CertificateReportingService();
}

content::BrowserContext*
CertificateReportingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

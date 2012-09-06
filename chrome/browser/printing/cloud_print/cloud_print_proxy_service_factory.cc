// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
CloudPrintProxyService* CloudPrintProxyServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CloudPrintProxyService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

CloudPrintProxyServiceFactory* CloudPrintProxyServiceFactory::GetInstance() {
  return Singleton<CloudPrintProxyServiceFactory>::get();
}

CloudPrintProxyServiceFactory::CloudPrintProxyServiceFactory()
    : ProfileKeyedServiceFactory("CloudPrintProxyService",
                                 ProfileDependencyManager::GetInstance()) {
}

CloudPrintProxyServiceFactory::~CloudPrintProxyServiceFactory() {
}

ProfileKeyedService* CloudPrintProxyServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  CloudPrintProxyService* service = new CloudPrintProxyService(profile);
  service->Initialize();

  return service;
}

bool CloudPrintProxyServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/usb/usb_service.h"

UsbServiceFactory* UsbServiceFactory::GetInstance() {
  return Singleton<UsbServiceFactory>::get();
}

UsbService* UsbServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<UsbService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

UsbServiceFactory::UsbServiceFactory() : ProfileKeyedServiceFactory(
    "UsbService", ProfileDependencyManager::GetInstance()) {}

UsbServiceFactory::~UsbServiceFactory() {}

ProfileKeyedService* UsbServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new UsbService();
}

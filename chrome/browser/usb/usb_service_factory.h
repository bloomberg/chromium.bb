// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_USB_USB_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class UsbService;

class UsbServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static UsbService* GetForProfile(Profile* profile);
  static bool HasUsbService(Profile* profile);

  static UsbServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<UsbServiceFactory>;

  UsbServiceFactory();
  virtual ~UsbServiceFactory();

  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_USB_USB_SERVICE_FACTORY_H_

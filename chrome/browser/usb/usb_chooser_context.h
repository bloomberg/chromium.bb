// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "device/usb/usb_service.h"

class UsbChooserContext : public ChooserContextBase,
                          public device::UsbService::Observer {
 public:
  explicit UsbChooserContext(Profile* profile);
  ~UsbChooserContext() override;

  // Grants |requesting_origin| access to the USB device known to
  // device::UsbService as |guid|.
  void GrantDevicePermission(const GURL& requesting_origin,
                             const GURL& embedding_origin,
                             const std::string& guid);

  // Revokes |requesting_origin|'s access to the USB device known to
  // device::UsbService as |guid|.
  void RevokeDevicePermission(const GURL& requesting_origin,
                              const GURL& embedding_origin,
                              const std::string& guid);

  // Checks if |requesting_origin| (when embedded within |embedding_origin| has
  // access to the USB device known to device::UsbService as |guid|.
  bool HasDevicePermission(const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           const std::string& guid);

 private:
  // ChooserContextBase implementation.
  bool IsValidObject(const base::DictionaryValue& object) override;

  // device::UsbService::Observer implementation.
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

  std::map<GURL, std::set<std::string>> ephemeral_devices_;
  device::UsbService* usb_service_;
  ScopedObserver<device::UsbService, device::UsbService::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(UsbChooserContext);
};

#endif  // CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_

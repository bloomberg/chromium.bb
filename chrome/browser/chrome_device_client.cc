// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_device_client.h"

#include "base/logging.h"
#include "components/usb_service/usb_service.h"
#include "content/public/browser/browser_thread.h"

ChromeDeviceClient::ChromeDeviceClient() {}

ChromeDeviceClient::~ChromeDeviceClient() {}

usb_service::UsbService* ChromeDeviceClient::GetUsbService() {
  return usb_service::UsbService::GetInstance(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI));
}

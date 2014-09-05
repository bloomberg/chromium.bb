// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_device_client.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_service.h"
#include "device/usb/usb_service.h"

ChromeDeviceClient::ChromeDeviceClient() {}

ChromeDeviceClient::~ChromeDeviceClient() {}

device::UsbService* ChromeDeviceClient::GetUsbService() {
  return device::UsbService::GetInstance(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI));
}

device::HidService* ChromeDeviceClient::GetHidService() {
  return device::HidService::GetInstance(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI));
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_device_client.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_service.h"
#include "device/usb/usb_service.h"

using content::BrowserThread;

ChromeDeviceClient::ChromeDeviceClient() {}

ChromeDeviceClient::~ChromeDeviceClient() {}

device::UsbService* ChromeDeviceClient::GetUsbService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return device::UsbService::GetInstance(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

device::HidService* ChromeDeviceClient::GetHidService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return device::HidService::GetInstance(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

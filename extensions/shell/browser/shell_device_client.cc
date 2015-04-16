// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_device_client.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_service.h"
#include "device/usb/usb_service.h"

using content::BrowserThread;

namespace extensions {

ShellDeviceClient::ShellDeviceClient() {}

ShellDeviceClient::~ShellDeviceClient() {}

device::UsbService* ShellDeviceClient::GetUsbService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return device::UsbService::GetInstance(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

device::HidService* ShellDeviceClient::GetHidService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return device::HidService::GetInstance(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

}

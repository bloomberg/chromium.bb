// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/core/device_client.h"

#include "base/logging.h"

namespace device {

namespace {

DeviceClient* g_instance = NULL;

}  // namespace

DeviceClient::DeviceClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

DeviceClient::~DeviceClient() {
  g_instance = NULL;
}

/* static */
DeviceClient* DeviceClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

UsbService* DeviceClient::GetUsbService() {
  // This should never be called by clients which do not support the USB API.
  NOTREACHED();
  return NULL;
}

HidService* DeviceClient::GetHidService() {
  // This should never be called by clients which do not support the HID API.
  NOTREACHED();
  return NULL;
}

}  // namespace device

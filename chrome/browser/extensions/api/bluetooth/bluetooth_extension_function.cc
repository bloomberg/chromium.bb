// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_extension_function.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace {

const char kPlatformNotSupported[] =
    "This operation is not supported on your platform";

extensions::BluetoothEventRouter* GetEventRouter(
    content::BrowserContext* context) {
  return extensions::BluetoothAPI::Get(context)->bluetooth_event_router();
}

bool IsBluetoothSupported(content::BrowserContext* context) {
  return GetEventRouter(context)->IsBluetoothSupported();
}

void GetAdapter(const device::BluetoothAdapterFactory::AdapterCallback callback,
                content::BrowserContext* context) {
  GetEventRouter(context)->GetAdapter(callback);
}

}  // namespace

namespace extensions {

namespace api {

BluetoothExtensionFunction::BluetoothExtensionFunction() {
}

BluetoothExtensionFunction::~BluetoothExtensionFunction() {
}

bool BluetoothExtensionFunction::RunImpl() {
  if (!IsBluetoothSupported(browser_context())) {
    SetError(kPlatformNotSupported);
    return false;
  }
  GetAdapter(base::Bind(&BluetoothExtensionFunction::RunOnAdapterReady, this),
             browser_context());

  return true;
}

void BluetoothExtensionFunction::RunOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DoWork(adapter);
}

}  // namespace api

}  // namespace extensions

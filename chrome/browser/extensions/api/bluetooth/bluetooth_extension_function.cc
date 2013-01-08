// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_extension_function.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace {

const char kPlatformNotSupported[] =
    "This operation is not supported on your platform";

scoped_refptr<device::BluetoothAdapter> GetAdapter(Profile* profile) {
  extensions::ExtensionBluetoothEventRouter* event_router =
      extensions::BluetoothAPI::Get(profile)->bluetooth_event_router();
  return event_router->GetAdapter();
}

}  // namespace

namespace extensions {

namespace api {

BluetoothExtensionFunction::BluetoothExtensionFunction() {
}

BluetoothExtensionFunction::~BluetoothExtensionFunction() {
}

bool BluetoothExtensionFunction::RunImpl() {
  scoped_refptr<device::BluetoothAdapter> adapter = GetAdapter(profile());
  if (!adapter) {
    SetError(kPlatformNotSupported);
    return false;
  }
  DoWork(adapter);
  return true;
}

}  // namespace api

}  // namespace extensions

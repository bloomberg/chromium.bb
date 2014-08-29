// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_extension_function.h"

#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "extensions/browser/api/bluetooth/bluetooth_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"

using content::BrowserThread;

namespace {

const char kPlatformNotSupported[] =
    "This operation is not supported on your platform";

extensions::BluetoothEventRouter* GetEventRouter(
    content::BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return extensions::BluetoothAPI::Get(context)->event_router();
}

bool IsBluetoothSupported(content::BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return GetEventRouter(context)->IsBluetoothSupported();
}

void GetAdapter(const device::BluetoothAdapterFactory::AdapterCallback callback,
                content::BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GetEventRouter(context)->GetAdapter(callback);
}

}  // namespace

namespace extensions {
namespace core_api {

BluetoothExtensionFunction::BluetoothExtensionFunction() {
}

BluetoothExtensionFunction::~BluetoothExtensionFunction() {
}

bool BluetoothExtensionFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DoWork(adapter);
}

}  // namespace core_api
}  // namespace extensions

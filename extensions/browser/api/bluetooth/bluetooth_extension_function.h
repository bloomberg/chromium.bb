// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_function.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace extensions {
namespace core_api {

// Base class for bluetooth extension functions. This class initializes
// bluetooth adapter and calls (on the UI thread) DoWork() implemented by
// individual bluetooth extension functions.
class BluetoothExtensionFunction : public AsyncExtensionFunction {
 public:
  BluetoothExtensionFunction();

 protected:
  virtual ~BluetoothExtensionFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  void RunOnAdapterReady(scoped_refptr<device::BluetoothAdapter> adapter);

  // Implemented by individual bluetooth extension functions, called
  // automatically on the UI thread once |adapter| has been initialized.
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) = 0;

  DISALLOW_COPY_AND_ASSIGN(BluetoothExtensionFunction);
};

}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_

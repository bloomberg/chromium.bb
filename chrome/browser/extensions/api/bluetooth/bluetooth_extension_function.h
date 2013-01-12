// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_function.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace extensions {

namespace api {

// Base class for bluetooth extension functions. This class initializes
// bluetooth adapter and calls DoWork() implemented by individual bluetooth
// extension functions.
class BluetoothExtensionFunction : public AsyncExtensionFunction {
 public:
  BluetoothExtensionFunction();

 protected:
  virtual ~BluetoothExtensionFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void RunOnAdapterReady(scoped_refptr<device::BluetoothAdapter> adapter);

  // Implemented by individual bluetooth extension functions, called
  // automatically once |adapter| has been initialized.
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) = 0;

  base::WeakPtrFactory<BluetoothExtensionFunction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothExtensionFunction);
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_

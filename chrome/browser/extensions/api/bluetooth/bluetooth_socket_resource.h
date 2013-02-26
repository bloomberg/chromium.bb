// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_SOCKET_RESOURCE_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_SOCKET_RESOURCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace device {

class BluetoothSocket;

}  // namespace device

namespace extensions {

// A BluetoothSocketResource is an ApiResource wrapper for a BluetoothSocket.
class BluetoothSocketResource : public ApiResource {
 public:
  BluetoothSocketResource(const std::string& owner_extension_id,
                          scoped_refptr<device::BluetoothSocket> socket);
  virtual ~BluetoothSocketResource();

  scoped_refptr<device::BluetoothSocket> socket() {
    return socket_;
  }

 private:
  scoped_refptr<device::BluetoothSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketResource);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_SOCKET_RESOURCE_H_

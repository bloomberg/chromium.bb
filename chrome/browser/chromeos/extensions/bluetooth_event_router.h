// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_BLUETOOTH_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_BLUETOOTH_EVENT_ROUTER_H_
#pragma once

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

class ExtensionBluetoothEventRouter
    : public chromeos::BluetoothAdapter::Observer {
 public:
  explicit ExtensionBluetoothEventRouter(Profile* profile);
  virtual ~ExtensionBluetoothEventRouter();

  const chromeos::BluetoothAdapter* adapter() const { return adapter_.get(); }
  chromeos::BluetoothAdapter* GetMutableAdapter() { return adapter_.get(); }

  // Register the BluetoothSocket |socket| for use by the extensions system.
  // This class will hold onto the socket for its lifetime, or until
  // ReleaseSocket is called for the socket.  Returns an id for the socket.
  int RegisterSocket(scoped_refptr<BluetoothSocket> socket);

  // Release the BluetoothSocket corresponding to |id|.  Returns true if
  // the socket was found and released, false otherwise.
  bool ReleaseSocket(int id);

  // Get the BluetoothSocket corresponding to |id|.
  scoped_refptr<BluetoothSocket> GetSocket(int id);

  // Override from chromeos::BluetoothAdapter::Observer
  virtual void AdapterPresentChanged(
      chromeos::BluetoothAdapter* adapter, bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(
      chromeos::BluetoothAdapter* adapter, bool has_power) OVERRIDE;
 private:
  void DispatchEvent(const char* event_name, bool value);

  Profile* profile_;
  scoped_ptr<chromeos::BluetoothAdapter> adapter_;

  // The next id to use for referring to a BluetoothSocket.  We avoid using
  // the fd of the socket because we don't want to leak that information to
  // the extension javascript.
  int next_socket_id_;

  typedef std::map<int, scoped_refptr<BluetoothSocket> > SocketMap;
  SocketMap socket_map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBluetoothEventRouter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_BLUETOOTH_EVENT_ROUTER_H_

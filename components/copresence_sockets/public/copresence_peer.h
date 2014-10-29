// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_SOCKETS_COPRESENCE_PEER_H_
#define COMPONENTS_COPRESENCE_SOCKETS_COPRESENCE_PEER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {
class BluetoothAdapter;
class BluetoothDevice;
class BluetoothSocket;
}

namespace copresence_sockets {

class CopresenceSocket;

// A CopresencePeer is an object that can be connected to. Creating a peer
// will create a server that will listen on a medium (currently only Bluetooth)
// and accept connections.
class CopresencePeer {
 public:
  // Callback with the locator data for the created peer. On a failed create,
  // the locator string will be empty.
  typedef base::Callback<void(const std::string&)> CreatePeerCallback;

  // Callback that accepts connections. The callback takes ownership of the
  // Copresence socket passed in the parameter.
  typedef base::Callback<void(scoped_ptr<CopresenceSocket> socket)>
      AcceptCallback;

  // Create a CopresencePeer and start listening for connections. Once the peer
  // object is created, the locator data for the object is returned via
  // create_callback. This locator data can be used to connect to this peer by
  // remote CopresenceSockets. Any connections accepted by this peer are
  // delivered as CopresenceSocket objects to accept_callback.
  CopresencePeer(const CreatePeerCallback& create_callback,
                 const AcceptCallback& accept_callback);

  virtual ~CopresencePeer();

  // This will return a string containing the data needed for a remote
  // CopresenceSocket to connect to this peer.
  std::string GetLocatorData();

 private:
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void OnCreateService(scoped_refptr<device::BluetoothSocket> socket);
  void OnCreateServiceError(const std::string& message);

  void OnAccept(const device::BluetoothDevice* device,
                scoped_refptr<device::BluetoothSocket> socket);
  void OnAcceptError(const std::string& message);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<device::BluetoothSocket> server_socket_;

  CreatePeerCallback create_callback_;
  AcceptCallback accept_callback_;

  scoped_ptr<device::BluetoothDevice::PairingDelegate> delegate_;

  base::WeakPtrFactory<CopresencePeer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CopresencePeer);
};

}  // namespace copresence_sockets

#endif  // COMPONENTS_COPRESENCE_SOCKETS_COPRESENCE_PEER_H_

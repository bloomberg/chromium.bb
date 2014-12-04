// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINT_H_
#define COMPONENTS_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/copresence_endpoints/copresence_socket.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {
class BluetoothAdapter;
class BluetoothSocket;
}

namespace copresence_endpoints {

// A CopresenceEndpoint is an object that can be used for communication with
// remote endpoints. Creating this object will create a server that will listen
// on Bluetooth (currently) and accept connections. Once a connection is
// accepted, the endpoint continuously listens for data on the accepted
// connection(s).
class CopresenceEndpoint {
 public:
  // Callback with the locator data for the created peer. On a failed create,
  // the locator string will be empty.
  typedef base::Callback<void(const std::string&)> CreateEndpointCallback;

  // Create a CopresenceEndpoint and start listening for connections. Once the
  // endpoint object is created, the locator data for the object is returned via
  // create_callback. This locator data can be used to connect to this peer by
  // a remote endpoint.
  // endpoint_id is the id that this endpoint will use to identify itself.
  // create_callback is called when the endpoint creation is complete.
  // accept_callback is called when we receive an incoming connection.
  // receive_callback is called when we receive data on this endpoint.
  CopresenceEndpoint(int endpoint_id,
                     const CreateEndpointCallback& create_callback,
                     const base::Closure& accept_callback,
                     const CopresenceSocket::ReceiveCallback& receive_callback);

  virtual ~CopresenceEndpoint();

  // Send's data to the remote device connected to this endpoint.
  bool Send(const scoped_refptr<net::IOBuffer>& buffer, int buffer_size);

  // This will return a string containing the data needed for a remote endpoint
  // to connect to this endpoint.
  std::string GetLocator();

 private:
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void OnCreateService(scoped_refptr<device::BluetoothSocket> socket);
  void OnCreateServiceError(const std::string& message);

  void OnAccept(const device::BluetoothDevice* device,
                scoped_refptr<device::BluetoothSocket> socket);
  void OnAcceptError(const std::string& message);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<device::BluetoothSocket> server_socket_;
  // TODO(rkc): Eventually an endpoint will be able to accept multiple
  // connections. Whenever the API supports one-to-many connections, fix this
  // code to do so too.
  scoped_ptr<CopresenceSocket> client_socket_;

  int endpoint_id_;
  CreateEndpointCallback create_callback_;
  base::Closure accept_callback_;
  CopresenceSocket::ReceiveCallback receive_callback_;

  scoped_ptr<device::BluetoothDevice::PairingDelegate> delegate_;

  base::WeakPtrFactory<CopresenceEndpoint> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceEndpoint);
};

}  // namespace copresence_endpoints

#endif  // COMPONENTS_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINT_H_

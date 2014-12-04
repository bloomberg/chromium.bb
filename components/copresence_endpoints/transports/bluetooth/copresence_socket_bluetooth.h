// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_SOCKETS_TRANSPORTS_COPRESENCE_SOCKET_BLUETOOTH_H_
#define COMPONENTS_COPRESENCE_SOCKETS_TRANSPORTS_COPRESENCE_SOCKET_BLUETOOTH_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/copresence_endpoints/copresence_socket.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace net {
class IOBuffer;
}

namespace copresence_endpoints {

// A CopresenceSocketBluetooth is the Bluetooth implementation of a
// CopresenceSocket. This is currently a thin wrapper around BluetoothSocket.
class CopresenceSocketBluetooth : public CopresenceSocket {
 public:
  explicit CopresenceSocketBluetooth(
      const scoped_refptr<device::BluetoothSocket>& socket);
  ~CopresenceSocketBluetooth() override;

  // CopresenceSocket overrides:
  bool Send(const scoped_refptr<net::IOBuffer>& buffer,
            int buffer_size) override;
  void Receive(const ReceiveCallback& callback) override;

 private:
  void OnSendComplete(int status);
  void OnSendError(const std::string& message);

  void OnReceive(int size, scoped_refptr<net::IOBuffer> io_buffer);
  void OnReceiveError(device::BluetoothSocket::ErrorReason reason,
                      const std::string& message);

  scoped_refptr<device::BluetoothSocket> socket_;
  ReceiveCallback receive_callback_;

  bool receiving_;

  base::WeakPtrFactory<CopresenceSocketBluetooth> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceSocketBluetooth);
};

}  // namespace copresence_endpoints

#endif  // COMPONENTS_COPRESENCE_SOCKETS_TRANSPORTS_COPRESENCE_SOCKET_BLUETOOTH_H_

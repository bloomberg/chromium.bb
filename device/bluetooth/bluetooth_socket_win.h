// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_

#include <WinSock2.h>

#include <string>

#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_socket.h"

namespace device {

class BluetoothServiceRecord;

// The BluetoothSocketWin class implements BluetoothSocket for the Microsoft
// Windows platform.
class BluetoothSocketWin : public BluetoothSocketNet {
 public:
  typedef base::Callback<void(scoped_refptr<BluetoothSocketWin>,
                              const net::IPEndPoint&)> OnNewConnectionCallback;

  static scoped_refptr<BluetoothSocketWin> CreateBluetoothSocket(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<BluetoothSocketThread> socket_thread,
      net::NetLog* net_log,
      const net::NetLog::Source& source);

  // Starts a service with the given uuid, name and rfcomm_channel.
  // |success_callback| is invoked when the underlying socket is created
  // and the service is published successfully. Otherwise, |error_callback| is
  // called with an error message. |new_connection_callback| is invoked when
  // an incoming connection is accepted by the underlying socket.
  void StartService(
      const BluetoothUUID& uuid,
      const std::string& name,
      int rfcomm_channel,
      const base::Closure& success_callback,
      const ErrorCompletionCallback& error_callback,
      const OnNewConnectionCallback& new_connection_callback);

  // connection has been established successfully. If an error occurs, calls
  // |error_callback| with a system error message.
  void Connect(const BluetoothServiceRecord& service_record,
               const base::Closure& success_callback,
               const ErrorCompletionCallback& error_callback);

  // BluetoothSocketNet:
  void ResetData();

  // BluetoothSocket:
  virtual void Accept(const AcceptCompletionCallback& success_callback,
                      const ErrorCompletionCallback& error_callback) OVERRIDE;


 protected:
  virtual ~BluetoothSocketWin();

 private:
  struct ServiceRegData;

  BluetoothSocketWin(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     scoped_refptr<BluetoothSocketThread> socket_thread,
                     net::NetLog* net_log,
                     const net::NetLog::Source& source);


  void DoConnect(const base::Closure& success_callback,
                 const ErrorCompletionCallback& error_callback);
  void DoStartService(const BluetoothUUID& uuid,
      const std::string& name,
      int rfcomm_channel,
      const base::Closure& success_callback,
      const ErrorCompletionCallback& error_callback,
      const OnNewConnectionCallback& new_connection_callback);
  void DoAccept();
  void OnAcceptOnSocketThread(int accept_result);
  void OnAcceptOnUI(scoped_ptr<net::TCPSocket> accept_socket,
                    const net::IPEndPoint& peer_address);

  std::string device_address_;
  bool supports_rfcomm_;
  uint8 rfcomm_channel_;
  BTH_ADDR bth_addr_;

  scoped_ptr<ServiceRegData> service_reg_data_;
  scoped_ptr<net::TCPSocket> accept_socket_;
  net::IPEndPoint accept_address_;
  OnNewConnectionCallback on_new_connection_callback_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_

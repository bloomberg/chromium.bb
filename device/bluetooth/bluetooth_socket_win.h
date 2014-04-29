// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_

#include <WinSock2.h>

#include <queue>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_log.h"
#include "net/socket/tcp_socket.h"

namespace net {
class IOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace device {

class BluetoothServiceRecord;
class BluetoothSocketThreadWin;

// This class is an implementation of BluetoothSocket class for the Windows
// platform.  All public methods (including the factory method) must be called
// on the UI thread, while underlying socket operations are performed on a
// separated thread.
class BluetoothSocketWin : public BluetoothSocket {
 public:
  typedef base::Callback<void(scoped_refptr<BluetoothSocketWin>,
                              const net::IPEndPoint&)> OnNewConnectionCallback;

  static scoped_refptr<BluetoothSocketWin> CreateBluetoothSocket(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<BluetoothSocketThreadWin> socket_thread,
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

  // Overriden from BluetoothSocket:
  virtual void Close() OVERRIDE;

  virtual void Disconnect(const base::Closure& callback) OVERRIDE;

  virtual void Receive(int buffer_size,
                       const ReceiveCompletionCallback& success_callback,
                       const ReceiveErrorCompletionCallback& error_callback)
      OVERRIDE;
  virtual void Send(scoped_refptr<net::IOBuffer> buffer,
                    int buffer_size,
                    const SendCompletionCallback& success_callback,
                    const ErrorCompletionCallback& error_callback) OVERRIDE;

 protected:
  virtual ~BluetoothSocketWin();

 private:
  struct ServiceRegData;

  struct WriteRequest {
    scoped_refptr<net::IOBuffer> buffer;
    int buffer_size;
    SendCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
  };

  BluetoothSocketWin(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     scoped_refptr<BluetoothSocketThreadWin> socket_thread,
                     net::NetLog* net_log,
                     const net::NetLog::Source& source);

  void DoClose();
  void DoConnect(const base::Closure& success_callback,
                 const ErrorCompletionCallback& error_callback);
  void DoDisconnect(const base::Closure& callback);
  void DoReceive(int buffer_size,
                 const ReceiveCompletionCallback& success_callback,
                 const ReceiveErrorCompletionCallback& error_callback);
  void DoSend(scoped_refptr<net::IOBuffer> buffer,
              int buffer_size,
              const SendCompletionCallback& success_callback,
              const ErrorCompletionCallback& error_callback);

  void PostSuccess(const base::Closure& callback);
  void PostErrorCompletion(const ErrorCompletionCallback& callback,
                           const std::string& error);
  void PostReceiveCompletion(const ReceiveCompletionCallback& callback,
                             int io_buffer_size,
                             scoped_refptr<net::IOBuffer> io_buffer);
  void PostReceiveErrorCompletion(
      const ReceiveErrorCompletionCallback& callback,
      ErrorReason reason,
      const std::string& error_message);
  void PostSendCompletion(const SendCompletionCallback& callback,
                          int bytes_written);

  void SendFrontWriteRequest();
  void OnSocketWriteComplete(const SendCompletionCallback& success_callback,
                             const ErrorCompletionCallback& error_callback,
                             int net_status);
  void OnSocketReadComplete(
      const ReceiveCompletionCallback& success_callback,
      const ReceiveErrorCompletionCallback& error_callback,
      int send_result);

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

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothSocketThreadWin> socket_thread_;
  net::NetLog* net_log_;
  const net::NetLog::Source source_;
  std::string device_address_;
  bool supports_rfcomm_;
  uint8 rfcomm_channel_;
  BTH_ADDR bth_addr_;
  scoped_ptr<net::TCPSocket> tcp_socket_;
  // Queue of pending writes. The buffer at the front of the queue is the one
  // being written.
  std::queue<linked_ptr<WriteRequest> > write_queue_;
  scoped_refptr<net::IOBufferWithSize> read_buffer_;

  scoped_ptr<ServiceRegData> service_reg_data_;
  scoped_ptr<net::TCPSocket> accept_socket_;
  net::IPEndPoint accept_address_;
  OnNewConnectionCallback on_new_connection_callback_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_

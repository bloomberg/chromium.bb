// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_

#include <queue>
#include <string>

#include <IOKit/IOreturn.h>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_socket.h"

#ifdef __OBJC__
@class BluetoothRFCOMMChannelDelegate;
@class IOBluetoothRFCOMMChannel;
@class IOBluetoothSDPServiceRecord;
#else
class BluetoothRFCOMMChannelDelegate;
class IOBluetoothRFCOMMChannel;
class IOBluetoothSDPServiceRecord;
#endif

namespace net {
class IOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace device {

class BluetoothServiceRecord;

// This class is an implementation of BluetoothSocket class for OSX platform.
// All methods of this class must all be called on the UI thread, as per Chrome
// guidelines on performing Async IO on UI thread on MacOS.
class BluetoothSocketMac : public BluetoothSocket {
 public:
  static scoped_refptr<BluetoothSocketMac> CreateBluetoothSocket(
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      IOBluetoothSDPServiceRecord* record);

  // Connect to the peer device and calls |success_callback| when the
  // connection has been established successfully. If an error occurs, calls
  // |error_callback| with a system error message.
  void Connect(const base::Closure& success_callback,
               const ErrorCompletionCallback& error_callback);

  // Overriden from BluetoothSocket:
  virtual void Close() OVERRIDE;
  virtual void Disconnect(const base::Closure& callback) OVERRIDE;
  virtual void Receive(int count,
                       const ReceiveCompletionCallback& success_callback,
                       const ReceiveErrorCompletionCallback& error_callback)
      OVERRIDE;
  virtual void Send(scoped_refptr<net::IOBuffer> buffer,
                    int buffer_size,
                    const SendCompletionCallback& success_callback,
                    const ErrorCompletionCallback& error_callback) OVERRIDE;

  // called by BluetoothRFCOMMChannelDelegate.
  void OnChannelOpened(IOBluetoothRFCOMMChannel* rfcomm_channel,
                       IOReturn status);

  // called by BluetoothRFCOMMChannelDelegate.
  void OnChannelClosed(IOBluetoothRFCOMMChannel* rfcomm_channel);

  // called by BluetoothRFCOMMChannelDelegate.
  void OnChannelDataReceived(IOBluetoothRFCOMMChannel* rfcomm_channel,
                             void* data,
                             size_t length);

  // called by BluetoothRFCOMMChannelDelegate.
  void OnChannelWriteComplete(IOBluetoothRFCOMMChannel* rfcomm_channel,
                              void* refcon,
                              IOReturn status);

 protected:
  virtual ~BluetoothSocketMac();

 private:
  BluetoothSocketMac(
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      IOBluetoothSDPServiceRecord* record);

  struct SendRequest {
    SendRequest();
    ~SendRequest();
    int buffer_size;
    SendCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
    IOReturn status;
    int active_async_writes;
    bool error_signaled;
  };

  struct ReceiveCallbacks {
    ReceiveCallbacks();
    ~ReceiveCallbacks();
    ReceiveCompletionCallback success_callback;
    ReceiveErrorCompletionCallback error_callback;
  };

  struct ConnectCallbacks {
    ConnectCallbacks();
    ~ConnectCallbacks();
    base::Closure success_callback;
    ErrorCompletionCallback error_callback;
  };

  void ReleaseChannel();

  bool connecting() const { return connect_callbacks_; }

  // Used to verify all methods are called on the same thread.
  base::ThreadChecker thread_checker_;
  // Task Runner for the UI thread, used to post tasks.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  // (retained) The Bluetooth Service definition.
  IOBluetoothSDPServiceRecord* record_;
  // (weak) The RFCOMM channel delegate. Released when the channel is closed.
  BluetoothRFCOMMChannelDelegate* delegate_;
  // (retained) The IOBluetooth RFCOMM channel used to issue commands.
  IOBluetoothRFCOMMChannel* rfcomm_channel_;
  // Connection callbacks -- when a pending async connection is active.
  scoped_ptr<ConnectCallbacks> connect_callbacks_;
  // Packets received while there is no pending "receive" callback.
  std::queue<scoped_refptr<net::IOBufferWithSize> > receive_queue_;
  // Receive callbacks -- when a receive call is active.
  scoped_ptr<ReceiveCallbacks> receive_callbacks_;
  // Send queue -- one entry per pending send operation.
  std::queue<linked_ptr<SendRequest> > send_queue_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_

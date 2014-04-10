// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothRFCOMMChannel.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>

#include <limits>
#include <sstream>
#include <string>

#include "base/basictypes.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "device/bluetooth/bluetooth_service_record.h"
#include "device/bluetooth/bluetooth_service_record_mac.h"
#include "net/base/io_buffer.h"

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface IOBluetoothDevice (LionSDKDeclarations)
- (NSString*)addressString;
@end

#endif  // MAC_OS_X_VERSION_10_7

@interface BluetoothRFCOMMChannelDelegate
    : NSObject <IOBluetoothRFCOMMChannelDelegate> {
 @private
  device::BluetoothSocketMac* socket_;  // weak
}

- (id)initWithSocket:(device::BluetoothSocketMac*)socket;

@end

@implementation BluetoothRFCOMMChannelDelegate

- (id)initWithSocket:(device::BluetoothSocketMac*)socket {
  if ((self = [super init]))
    socket_ = socket;

  return self;
}

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel
                           status:(IOReturn)error {
  socket_->OnChannelOpened(rfcommChannel, error);
}

- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel
                            refcon:(void*)refcon
                            status:(IOReturn)error {
  socket_->OnChannelWriteComplete(rfcommChannel, refcon, error);
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel
                     data:(void*)dataPointer
                   length:(size_t)dataLength {
  socket_->OnChannelDataReceived(rfcommChannel, dataPointer, dataLength);
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel {
  socket_->OnChannelClosed(rfcommChannel);
}

@end

namespace {

const char kL2CAPNotSupported[] = "Bluetooth L2CAP protocol is not supported";
const char kSocketConnecting[] = "The socket is currently connecting";
const char kSocketAlreadyConnected[] = "The socket is already connected";
const char kSocketNotConnected[] = "The socket is not connected";
const char kReceivePending[] = "A Receive operation is pending";

template <class T>
void empty_queue(std::queue<T>& queue) {
  std::queue<T> empty;
  std::swap(queue, empty);
}

}  // namespace

namespace device {

BluetoothSocketMac::SendRequest::SendRequest()
    : status(kIOReturnSuccess), active_async_writes(0), error_signaled(false) {}

BluetoothSocketMac::SendRequest::~SendRequest() {}

BluetoothSocketMac::ReceiveCallbacks::ReceiveCallbacks() {}

BluetoothSocketMac::ReceiveCallbacks::~ReceiveCallbacks() {}

BluetoothSocketMac::ConnectCallbacks::ConnectCallbacks() {}

BluetoothSocketMac::ConnectCallbacks::~ConnectCallbacks() {}

// static
scoped_refptr<BluetoothSocketMac> BluetoothSocketMac::CreateBluetoothSocket(
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    IOBluetoothSDPServiceRecord* record) {
  return new BluetoothSocketMac(ui_task_runner, record);
}

BluetoothSocketMac::BluetoothSocketMac(
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    IOBluetoothSDPServiceRecord* record)
    : ui_task_runner_(ui_task_runner),
      record_(record),
      delegate_([[BluetoothRFCOMMChannelDelegate alloc] initWithSocket:this]),
      rfcomm_channel_(nil) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  [record_ retain];
}

BluetoothSocketMac::~BluetoothSocketMac() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  ReleaseChannel();
  [delegate_ release];
  [record_ release];
}

void BluetoothSocketMac::ReleaseChannel() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (rfcomm_channel_ != nil) {
    [rfcomm_channel_ setDelegate:nil];
    [rfcomm_channel_ closeChannel];
    [rfcomm_channel_ release];
    rfcomm_channel_ = nil;
  }

  // Closing the channel above prevents the callback delegate from being called
  // so it is now safe to release all callback state.
  connect_callbacks_.reset(NULL);
  receive_callbacks_.reset(NULL);
  empty_queue(receive_queue_);
  empty_queue(send_queue_);
}

void BluetoothSocketMac::Connect(
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  if (connecting()) {
    error_callback.Run(kSocketConnecting);
    return;
  }

  if (rfcomm_channel_ != nil) {
    error_callback.Run(kSocketAlreadyConnected);
    return;
  }

  uint8 rfcomm_channel_id;
  IOReturn status = [record_ getRFCOMMChannelID:&rfcomm_channel_id];
  if (status != kIOReturnSuccess) {
    // TODO(youngki) add support for L2CAP sockets as well.
    error_callback.Run(kL2CAPNotSupported);
    return;
  }

  IOBluetoothDevice* device = [record_ device];
  IOBluetoothRFCOMMChannel* rfcomm_channel;
  status = [device openRFCOMMChannelAsync:&rfcomm_channel
                            withChannelID:rfcomm_channel_id
                                 delegate:delegate_];
  if (status != kIOReturnSuccess) {
    std::stringstream error;
    error << std::string("Failed to connect bluetooth socket (")
          << base::SysNSStringToUTF8([device addressString]) << "): (" << status
          << std::string(")");
    error_callback.Run(error.str());
    return;
  }

  connect_callbacks_.reset(new ConnectCallbacks());
  connect_callbacks_->success_callback = success_callback;
  connect_callbacks_->error_callback = error_callback;
  rfcomm_channel_ = rfcomm_channel;
  [rfcomm_channel_ setDelegate:delegate_];
}

void BluetoothSocketMac::OnChannelOpened(
    IOBluetoothRFCOMMChannel* rfcomm_channel,
    IOReturn status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(rfcomm_channel_ == rfcomm_channel);
  DCHECK(connecting());

  scoped_ptr<ConnectCallbacks> temp = connect_callbacks_.Pass();
  if (status != kIOReturnSuccess) {
    ReleaseChannel();
    std::stringstream error;
    error << "Failed to connect bluetooth socket ("
          << base::SysNSStringToUTF8([[record_ device] addressString]) << "): ("
          << status << ")";
    temp->error_callback.Run(error.str());
    return;
  }

  temp->success_callback.Run();
}

void BluetoothSocketMac::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  ReleaseChannel();
}

void BluetoothSocketMac::Disconnect(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  ReleaseChannel();
  callback.Run();
}

void BluetoothSocketMac::Receive(
    int count,
    const ReceiveCompletionCallback& success_callback,
    const ReceiveErrorCompletionCallback& error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  if (connecting()) {
    error_callback.Run(BluetoothSocket::kSystemError, kSocketConnecting);
    return;
  }

  if (rfcomm_channel_ == nil) {
    error_callback.Run(BluetoothSocket::kDisconnected, kSocketNotConnected);
    return;
  }

  // Only one pending read at a time
  if (receive_callbacks_) {
    error_callback.Run(BluetoothSocket::kIOPending, kReceivePending);
    return;
  }

  // If there is at least one packet, consume it and succeed right away.
  if (!receive_queue_.empty()) {
    scoped_refptr<net::IOBufferWithSize> buffer = receive_queue_.front();
    receive_queue_.pop();
    success_callback.Run(buffer->size(), buffer);
    return;
  }

  // Set the receive callback to use when data is received.
  receive_callbacks_.reset(new ReceiveCallbacks());
  receive_callbacks_->success_callback = success_callback;
  receive_callbacks_->error_callback = error_callback;
}

void BluetoothSocketMac::OnChannelDataReceived(
    IOBluetoothRFCOMMChannel* rfcomm_channel,
    void* data,
    size_t length) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(rfcomm_channel_ == rfcomm_channel);
  DCHECK(!connecting());
  CHECK_LT(length, static_cast<size_t>(std::numeric_limits<int>::max()));

  int data_size = static_cast<int>(length);
  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(data_size));
  memcpy(buffer->data(), data, buffer->size());

  // If there is a pending read callback, call it now.
  if (receive_callbacks_) {
    scoped_ptr<ReceiveCallbacks> temp = receive_callbacks_.Pass();
    temp->success_callback.Run(buffer->size(), buffer);
    return;
  }

  // Otherwise, enqueue the buffer for later use
  receive_queue_.push(buffer);
}

void BluetoothSocketMac::Send(scoped_refptr<net::IOBuffer> buffer,
                              int buffer_size,
                              const SendCompletionCallback& success_callback,
                              const ErrorCompletionCallback& error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  if (connecting()) {
    error_callback.Run(kSocketConnecting);
    return;
  }

  if (rfcomm_channel_ == nil) {
    error_callback.Run(kSocketNotConnected);
    return;
  }

  // Create and enqueue request in preparation of async writes.
  linked_ptr<SendRequest> request(new SendRequest());
  request->buffer_size = buffer_size;
  request->success_callback = success_callback;
  request->error_callback = error_callback;
  send_queue_.push(request);

  // |writeAsync| accepts buffers of max. mtu bytes per call, so we need to emit
  // multiple write operations if buffer_size > mtu.
  BluetoothRFCOMMMTU mtu = [rfcomm_channel_ getMTU];
  scoped_refptr<net::DrainableIOBuffer> send_buffer(
      new net::DrainableIOBuffer(buffer, buffer_size));
  while (send_buffer->BytesRemaining() > 0) {
    int byte_count = send_buffer->BytesRemaining();
    if (byte_count > mtu)
      byte_count = mtu;
    IOReturn status = [rfcomm_channel_ writeAsync:send_buffer->data()
                                           length:byte_count
                                           refcon:request.get()];
    if (status != kIOReturnSuccess) {
      std::stringstream error;
      error << "Failed to connect bluetooth socket ("
            << base::SysNSStringToUTF8([[record_ device] addressString])
            << "): (" << status << ")";
      // Remember the first error only
      if (request->status == kIOReturnSuccess)
        request->status = status;
      request->error_signaled = true;
      request->error_callback.Run(error.str());
      // We may have failed to issue any write operation. In that case, there
      // will be no corresponding completion callback for this particular
      // request, so we must forget about it now.
      if (request->active_async_writes == 0) {
        send_queue_.pop();
      }
      return;
    }

    request->active_async_writes++;
    send_buffer->DidConsume(byte_count);
  }
}

void BluetoothSocketMac::OnChannelWriteComplete(
    IOBluetoothRFCOMMChannel* rfcomm_channel,
    void* refcon,
    IOReturn status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  // Note: We use "CHECK" below to ensure we never run into unforeseen
  // occurrences of asynchronous callbacks, which could lead to data
  // corruption.
  CHECK(rfcomm_channel_ == rfcomm_channel);
  CHECK(static_cast<SendRequest*>(refcon) == send_queue_.front().get());

  // Keep a local linked_ptr to avoid releasing the request too early if we end
  // up removing it from the queue.
  linked_ptr<SendRequest> request = send_queue_.front();

  // Remember the first error only
  if (status != kIOReturnSuccess) {
    if (request->status == kIOReturnSuccess)
      request->status = status;
  }

  // Figure out if we are done with this async request
  request->active_async_writes--;
  if (request->active_async_writes > 0)
    return;

  // If this was the last active async write for this request, remove it from
  // the queue and call the appropriate callback associated to the request.
  send_queue_.pop();
  if (request->status != kIOReturnSuccess) {
    if (!request->error_signaled) {
      std::stringstream error;
      error << "Failed to connect bluetooth socket ("
            << base::SysNSStringToUTF8([[record_ device] addressString])
            << "): (" << status << ")";
      request->error_signaled = true;
      request->error_callback.Run(error.str());
    }
  } else {
    request->success_callback.Run(request->buffer_size);
  }
}

void BluetoothSocketMac::OnChannelClosed(
    IOBluetoothRFCOMMChannel* rfcomm_channel) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(rfcomm_channel_ == rfcomm_channel);

  if (receive_callbacks_) {
    scoped_ptr<ReceiveCallbacks> temp = receive_callbacks_.Pass();
    temp->error_callback.Run(BluetoothSocket::kDisconnected,
                             kSocketNotConnected);
  }

  ReleaseChannel();
}

}  // namespace device

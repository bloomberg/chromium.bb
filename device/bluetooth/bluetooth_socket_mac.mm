// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothRFCOMMChannel.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>

#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
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

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel
                     data:(void*)dataPointer
                   length:(size_t)dataLength {
  socket_->OnDataReceived(rfcommChannel, dataPointer, dataLength);
}

@end

namespace device {

BluetoothSocketMac::BluetoothSocketMac(IOBluetoothRFCOMMChannel* rfcomm_channel)
    : rfcomm_channel_(rfcomm_channel),
      delegate_([[BluetoothRFCOMMChannelDelegate alloc] initWithSocket:this]) {
  [rfcomm_channel_ setDelegate:delegate_];
  ResetIncomingDataBuffer();
}

BluetoothSocketMac::~BluetoothSocketMac() {
  [rfcomm_channel_ setDelegate:nil];
  [rfcomm_channel_ closeChannel];
  [rfcomm_channel_ release];
  [delegate_ release];
}

// static
scoped_refptr<BluetoothSocket> BluetoothSocketMac::CreateBluetoothSocket(
    const BluetoothServiceRecord& service_record) {
  BluetoothSocketMac* bluetooth_socket = NULL;
  if (service_record.SupportsRfcomm()) {
    const BluetoothServiceRecordMac* service_record_mac =
        static_cast<const BluetoothServiceRecordMac*>(&service_record);
    IOBluetoothDevice* device = service_record_mac->GetIOBluetoothDevice();
    IOBluetoothRFCOMMChannel* rfcomm_channel;
    IOReturn status =
        [device openRFCOMMChannelAsync:&rfcomm_channel
                         withChannelID:service_record.rfcomm_channel()
                              delegate:nil];
    if (status == kIOReturnSuccess) {
      bluetooth_socket = new BluetoothSocketMac(rfcomm_channel);
    } else {
      LOG(ERROR) << "Failed to connect bluetooth socket ("
          << service_record.address() << "): (" << status << ")";
    }
  }
  // TODO(youngki): add support for L2CAP sockets as well.

  return scoped_refptr<BluetoothSocketMac>(bluetooth_socket);
}

// static
scoped_refptr<BluetoothSocket> BluetoothSocketMac::CreateBluetoothSocket(
    IOBluetoothSDPServiceRecord* record) {
  BluetoothSocketMac* bluetooth_socket = NULL;
  uint8 rfcomm_channel_id;
  if ([record getRFCOMMChannelID:&rfcomm_channel_id] == kIOReturnSuccess) {
    IOBluetoothDevice* device = [record device];
    IOBluetoothRFCOMMChannel* rfcomm_channel;
    IOReturn status =
        [device openRFCOMMChannelAsync:&rfcomm_channel
                         withChannelID:rfcomm_channel_id
                              delegate:nil];
    if (status == kIOReturnSuccess) {
      bluetooth_socket = new BluetoothSocketMac(rfcomm_channel);
    } else {
      LOG(ERROR) << "Failed to connect bluetooth socket ("
          << base::SysNSStringToUTF8([device addressString]) << "): (" << status
          << ")";
    }
  }

  // TODO(youngki): Add support for L2CAP sockets as well.

  return scoped_refptr<BluetoothSocketMac>(bluetooth_socket);
}

bool BluetoothSocketMac::Receive(net::GrowableIOBuffer* buffer) {
  CHECK(buffer->offset() == 0);
  int length = incoming_data_buffer_->offset();
  if (length > 0) {
    buffer->SetCapacity(length);
    memcpy(buffer->data(), incoming_data_buffer_->StartOfBuffer(), length);
    buffer->set_offset(length);

    ResetIncomingDataBuffer();
  }
  return true;
}

bool BluetoothSocketMac::Send(net::DrainableIOBuffer* buffer) {
  int bytes_written = buffer->BytesRemaining();
  IOReturn status = [rfcomm_channel_ writeAsync:buffer->data()
                                         length:bytes_written
                                         refcon:nil];
  if (status != kIOReturnSuccess) {
    error_message_ = base::StringPrintf(
        "Failed to send data. IOReturn code: %u", status);
    return false;
  }

  buffer->DidConsume(bytes_written);
  return true;
}

std::string BluetoothSocketMac::GetLastErrorMessage() const {
  return error_message_;
}

void BluetoothSocketMac::OnDataReceived(
    IOBluetoothRFCOMMChannel* rfcomm_channel, void* data, size_t length) {
  DCHECK(rfcomm_channel_ == rfcomm_channel);
  CHECK_LT(length, static_cast<size_t>(std::numeric_limits<int>::max()));
  int data_size = static_cast<int>(length);
  if (incoming_data_buffer_->RemainingCapacity() < data_size) {
    int additional_capacity =
        std::max(data_size, incoming_data_buffer_->capacity());
    CHECK_LT(
        additional_capacity,
        std::numeric_limits<int>::max() - incoming_data_buffer_->capacity());
    incoming_data_buffer_->SetCapacity(
        incoming_data_buffer_->capacity() + additional_capacity);
  }
  memcpy(incoming_data_buffer_->data(), data, data_size);
  incoming_data_buffer_->set_offset(
      incoming_data_buffer_->offset() + data_size);
}

void BluetoothSocketMac::ResetIncomingDataBuffer() {
  incoming_data_buffer_ = new net::GrowableIOBuffer();
  incoming_data_buffer_->SetCapacity(1024);
}

}  // namespace device

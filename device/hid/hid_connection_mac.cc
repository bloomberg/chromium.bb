// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_mac.h"

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/message_loop/message_loop.h"
#include "device/hid/hid_connection_mac.h"

namespace device {

HidConnectionMac::HidConnectionMac(HidDeviceInfo device_info)
    : HidConnection(device_info),
      device_(device_info.device_id, base::scoped_policy::RETAIN) {
  message_loop_ = base::MessageLoopProxy::current();

  DCHECK(device_.get());

  size_t expected_report_size = device_info.max_input_report_size;
  if (device_info.has_report_id) {
    expected_report_size++;
  }
  inbound_buffer_.resize(expected_report_size);
  if (inbound_buffer_.size() > 0) {
    IOHIDDeviceRegisterInputReportCallback(
        device_.get(),
        &inbound_buffer_[0],
        inbound_buffer_.size(),
        &HidConnectionMac::InputReportCallback,
        this);
  }
}

HidConnectionMac::~HidConnectionMac() {
  if (inbound_buffer_.size() > 0) {
    // Unregister the input report callback before this object is freed.
    IOHIDDeviceRegisterInputReportCallback(
        device_.get(), &inbound_buffer_[0], inbound_buffer_.size(), NULL, this);
  }
  Flush();
}

void HidConnectionMac::PlatformRead(const ReadCallback& callback) {
  if (!device_) {
    callback.Run(false, NULL, 0);
    return;
  }

  PendingHidRead pending_read;
  pending_read.callback = callback;
  pending_reads_.push(pending_read);
  ProcessReadQueue();
}

void HidConnectionMac::PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                                     size_t size,
                                     const WriteCallback& callback) {
  WriteReport(kIOHIDReportTypeOutput, buffer, size, callback);
}

void HidConnectionMac::PlatformGetFeatureReport(uint8_t report_id,
                                                const ReadCallback& callback) {
  if (!device_) {
    callback.Run(false, NULL, 0);
    return;
  }

  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(device_info().max_feature_report_size));
  CFIndex report_size = buffer->size();
  IOReturn result =
      IOHIDDeviceGetReport(device_,
                           kIOHIDReportTypeFeature,
                           report_id,
                           reinterpret_cast<uint8_t*>(buffer->data()),
                           &report_size);
  if (result == kIOReturnSuccess) {
    callback.Run(true, buffer, report_size);
  } else {
    VLOG(1) << "Failed to get feature report: " << result;
    callback.Run(false, NULL, 0);
  }
}

void HidConnectionMac::PlatformSendFeatureReport(
    scoped_refptr<net::IOBuffer> buffer,
    size_t size,
    const WriteCallback& callback) {
  WriteReport(kIOHIDReportTypeFeature, buffer, size, callback);
}

void HidConnectionMac::InputReportCallback(void* context,
                                           IOReturn result,
                                           void* sender,
                                           IOHIDReportType type,
                                           uint32_t report_id,
                                           uint8_t* report_bytes,
                                           CFIndex report_length) {
  if (result != kIOReturnSuccess) {
    VLOG(1) << "Failed to read input report: " << result;
    return;
  }

  HidConnectionMac* connection = static_cast<HidConnectionMac*>(context);
  scoped_refptr<net::IOBufferWithSize> buffer;
  if (connection->device_info().has_report_id) {
    // report_id is already contained in report_bytes
    buffer = new net::IOBufferWithSize(report_length);
    memcpy(buffer->data(), report_bytes, report_length);
  } else {
    buffer = new net::IOBufferWithSize(report_length + 1);
    buffer->data()[0] = 0;
    memcpy(buffer->data() + 1, report_bytes, report_length);
  }

  connection->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&HidConnectionMac::ProcessInputReport, connection, buffer));
}

void HidConnectionMac::WriteReport(IOHIDReportType type,
                                   scoped_refptr<net::IOBuffer> buffer,
                                   size_t size,
                                   const WriteCallback& callback) {
  if (!device_) {
    callback.Run(false);
    return;
  }

  uint8_t* data = reinterpret_cast<uint8_t*>(buffer->data());
  DCHECK(size >= 1);
  uint8_t report_id = data[0];
  if (report_id == 0) {
    // OS X only expects the first byte of the buffer to be the report ID if the
    // report ID is non-zero.
    ++data;
    --size;
  }

  IOReturn res =
      IOHIDDeviceSetReport(device_.get(), type, report_id, data, size);
  if (res == kIOReturnSuccess) {
    callback.Run(true);
  } else {
    VLOG(1) << "Failed to set report: " << res;
    callback.Run(false);
  }
}

void HidConnectionMac::Flush() {
  while (!pending_reads_.empty()) {
    pending_reads_.front().callback.Run(false, NULL, 0);
    pending_reads_.pop();
  }
}

void HidConnectionMac::ProcessInputReport(
    scoped_refptr<net::IOBufferWithSize> buffer) {
  DCHECK(thread_checker().CalledOnValidThread());
  PendingHidReport report;
  report.buffer = buffer;
  report.size = buffer->size();
  pending_reports_.push(report);
  ProcessReadQueue();
}

void HidConnectionMac::ProcessReadQueue() {
  DCHECK(thread_checker().CalledOnValidThread());
  while (pending_reads_.size() && pending_reports_.size()) {
    PendingHidRead read = pending_reads_.front();
    PendingHidReport report = pending_reports_.front();

    pending_reports_.pop();
    if (CompleteRead(report.buffer, report.size, read.callback)) {
      pending_reads_.pop();
    }
  }
}

}  // namespace device

// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_mac.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/mac/foundation_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/tuple.h"
#include "device/hid/hid_service.h"
#include "device/hid/hid_service_mac.h"
#include "net/base/io_buffer.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

namespace device {

HidConnectionMac::HidConnectionMac(HidServiceMac* service,
                                   HidDeviceInfo device_info,
                                   IOHIDDeviceRef device)
    : HidConnection(device_info),
      service_(service),
      device_(device),
      disconnected_(false) {
  DCHECK(thread_checker_.CalledOnValidThread());

  message_loop_ = base::MessageLoopProxy::current();

  CFRetain(device);
  inbound_buffer_.reset((uint8_t*) malloc(device_info.input_report_size + 1));
  IOHIDDeviceRegisterInputReportCallback(
      device_.get(),
      inbound_buffer_.get(),
      device_info.input_report_size + 1,
      &HidConnectionMac::InputReportCallback,
      this);
  IOHIDDeviceOpen(device_, kIOHIDOptionsTypeNone);
}
HidConnectionMac::~HidConnectionMac() {
  DCHECK(thread_checker_.CalledOnValidThread());

  while (read_queue_.size()) {
    read_queue_.front().c.Run(false, 0);
    read_queue_.pop();
  }

  IOHIDDeviceClose(device_, kIOHIDOptionsTypeNone);
}

void HidConnectionMac::InputReportCallback(void * context,
                                           IOReturn result,
                                           void * sender,
                                           IOHIDReportType type,
                                           uint32_t reportID,
                                           uint8_t * report,
                                           CFIndex reportLength) {
  HidConnectionMac* connection = reinterpret_cast<HidConnectionMac*>(context);
  size_t length = reportLength + (reportID != 0);
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(length));
  if (reportID) {
    buffer->data()[0] = reportID;
    memcpy(buffer->data() + 1, report, reportLength);
  } else {
    memcpy(buffer->data(), report, reportLength);
  }
  connection->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&HidConnectionMac::ProcessInputReport,
                 connection,
                 type,
                 buffer,
                 length));
}

void HidConnectionMac::ProcessReadQueue() {
  DCHECK(thread_checker_.CalledOnValidThread());

  while(read_queue_.size() && input_reports_.size()) {
    PendingRead read = read_queue_.front();
    read_queue_.pop();
    PendingReport report = input_reports_.front();

    if (read.b < report.second) {
      read.c.Run(false, report.second);
    } else {
      memcpy(read.a->data(), report.first->data(), report.second);
      input_reports_.pop();
      read.c.Run(true, report.second);
    }
  }
}

void HidConnectionMac::ProcessInputReport(IOHIDReportType type,
                                          scoped_refptr<net::IOBuffer> report,
                                          CFIndex reportLength) {
  DCHECK(thread_checker_.CalledOnValidThread());

  input_reports_.push(std::make_pair(report, reportLength));
  ProcessReadQueue();
}

void HidConnectionMac::WriteReport(IOHIDReportType type,
                                   scoped_refptr<net::IOBuffer> buffer,
                                   size_t size,
                                   const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (disconnected_ || !device_) {
    callback.Run(false, 0);
    return;
  }
  const unsigned char* data_to_send =
      reinterpret_cast<const unsigned char*>(buffer->data());
  size_t length_to_send = size;
  if (data_to_send[0] == 0x0) {
      /* Not using numbered Reports.
       Don't send the report number. */
      ++data_to_send;
      --length_to_send;
  }
  IOReturn res = IOHIDDeviceSetReport(device_.get(),
                                      type,
                                      buffer->data()[0], /* Report ID*/
                                      data_to_send,
                                      length_to_send);
  if (res != kIOReturnSuccess) {
    callback.Run(false, 0);
  } else {
    callback.Run(true, size);
  }
}

void HidConnectionMac::Read(scoped_refptr<net::IOBuffer> buffer,
                            size_t size,
                            const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (disconnected_ || !device_) {
    callback.Run(false, 0);
    return;
  }
  read_queue_.push(MakeTuple(buffer, size, callback));
  ProcessReadQueue();
}

void HidConnectionMac::Write(scoped_refptr<net::IOBuffer> buffer,
                             size_t size,
                             const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WriteReport(kIOHIDReportTypeOutput, buffer, size, callback);
}

void HidConnectionMac::SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                         size_t size,
                                         const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WriteReport(kIOHIDReportTypeFeature, buffer, size, callback);
}

void HidConnectionMac::GetFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                        size_t size,
                                        const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (disconnected_ || !device_ || device_info_.feature_report_size == 0) {
    callback.Run(false, 0);
    return;
  }

  if (device_info_.feature_report_size != 0 &&
      device_info_.feature_report_size != size) {
    callback.Run(false, 0);
    return;
  }

  CFIndex len = device_info_.feature_report_size;
  IOReturn res = IOHIDDeviceGetReport(device_,
                                      kIOHIDReportTypeFeature,
                                      0,
                                      (uint8_t*) buffer->data(),
                                      &len);
  if (res == kIOReturnSuccess)
    callback.Run(true, len);
  else
    callback.Run(false, 0);
}

}  // namespace device

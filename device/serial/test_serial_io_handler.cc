// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/test_serial_io_handler.h"

#include "base/bind.h"
#include "device/serial/serial.mojom.h"

namespace device {

TestSerialIoHandler::TestSerialIoHandler()
    : SerialIoHandler(NULL),
      opened_(false),
      dtr_(false),
      rts_(false),
      flushes_(0) {
}

scoped_refptr<SerialIoHandler> TestSerialIoHandler::Create() {
  return scoped_refptr<SerialIoHandler>(new TestSerialIoHandler);
}

void TestSerialIoHandler::Open(const std::string& port,
                               const OpenCompleteCallback& callback) {
  DCHECK(!opened_);
  opened_ = true;
  callback.Run(true);
}

bool TestSerialIoHandler::ConfigurePort(
    const serial::ConnectionOptions& options) {
  if (options.bitrate)
    info_.bitrate = options.bitrate;
  if (options.data_bits != serial::DATA_BITS_NONE)
    info_.data_bits = options.data_bits;
  if (options.parity_bit != serial::PARITY_BIT_NONE)
    info_.parity_bit = options.parity_bit;
  if (options.stop_bits != serial::STOP_BITS_NONE)
    info_.stop_bits = options.stop_bits;
  if (options.has_cts_flow_control)
    info_.cts_flow_control = options.cts_flow_control;
  return true;
}

void TestSerialIoHandler::ReadImpl() {
}

void TestSerialIoHandler::CancelReadImpl() {
  QueueReadCompleted(0, read_cancel_reason());
}

void TestSerialIoHandler::WriteImpl() {
  DCHECK(pending_read_buffer());
  DCHECK_LE(pending_write_buffer_len(), pending_read_buffer_len());
  memcpy(pending_read_buffer(),
         pending_write_buffer(),
         pending_write_buffer_len());
  QueueReadCompleted(pending_write_buffer_len(), serial::RECEIVE_ERROR_NONE);
  QueueWriteCompleted(pending_write_buffer_len(), serial::SEND_ERROR_NONE);
}

void TestSerialIoHandler::CancelWriteImpl() {
  QueueWriteCompleted(0, write_cancel_reason());
}

serial::DeviceControlSignalsPtr TestSerialIoHandler::GetControlSignals() const {
  serial::DeviceControlSignalsPtr signals(serial::DeviceControlSignals::New());
  *signals = device_control_signals_;
  return signals.Pass();
}

serial::ConnectionInfoPtr TestSerialIoHandler::GetPortInfo() const {
  serial::ConnectionInfoPtr info(serial::ConnectionInfo::New());
  *info = info_;
  return info.Pass();
}

bool TestSerialIoHandler::Flush() const {
  flushes_++;
  return true;
}

bool TestSerialIoHandler::SetControlSignals(
    const serial::HostControlSignals& signals) {
  if (signals.has_dtr)
    dtr_ = signals.dtr;
  if (signals.has_rts)
    rts_ = signals.rts;
  return true;
}

TestSerialIoHandler::~TestSerialIoHandler() {
}

}  // namespace device

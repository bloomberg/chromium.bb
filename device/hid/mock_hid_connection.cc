// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/mock_hid_connection.h"
#include "device/hid/hid_device_info.h"

namespace device {

MockHidConnection::MockHidConnection(scoped_refptr<HidDeviceInfo> device)
    : HidConnection(device) {}

MockHidConnection::~MockHidConnection() {}

// HidConnection implementation.
void MockHidConnection::PlatformClose() {}

void MockHidConnection::PlatformRead(ReadCallback callback) {
  const char data[] = "TestRead";
  auto buffer = base::MakeRefCounted<net::IOBuffer>(sizeof(data));
  // report_id.
  buffer->data()[0] = 1;
  memcpy(buffer->data() + 1, data, sizeof(data) - 1);
  std::move(callback).Run(true, buffer, sizeof(data));
}

void MockHidConnection::PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                                      size_t size,
                                      WriteCallback callback) {
  std::move(callback).Run(true);
}

void MockHidConnection::PlatformGetFeatureReport(uint8_t report_id,
                                                 ReadCallback callback) {
  const char data[] = "TestGetFeatureReport";
  auto buffer = base::MakeRefCounted<net::IOBuffer>(sizeof(data) - 1);
  memcpy(buffer->data(), data, sizeof(data) - 1);
  std::move(callback).Run(true, buffer, sizeof(data) - 1);
}

void MockHidConnection::PlatformSendFeatureReport(
    scoped_refptr<net::IOBuffer> buffer,
    size_t size,
    WriteCallback callback) {
  std::move(callback).Run(true);
}

}  // namespace device

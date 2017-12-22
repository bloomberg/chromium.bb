// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/mock_u2f_ble_connection.h"

#include <utility>

namespace device {

MockU2fBleConnection::MockU2fBleConnection(std::string device_address)
    : U2fBleConnection(std::move(device_address)) {}

MockU2fBleConnection::~MockU2fBleConnection() = default;

void MockU2fBleConnection::ReadControlPointLength(
    ControlPointLengthCallback callback) {
  ReadControlPointLengthPtr(&callback);
}

void MockU2fBleConnection::ReadServiceRevisions(
    ServiceRevisionsCallback callback) {
  ReadServiceRevisionsPtr(&callback);
}

void MockU2fBleConnection::WriteControlPoint(const std::vector<uint8_t>& data,
                                             WriteCallback callback) {
  WriteControlPointPtr(data, &callback);
}

void MockU2fBleConnection::WriteServiceRevision(
    ServiceRevision service_revision,
    WriteCallback callback) {
  WriteServiceRevisionPtr(service_revision, &callback);
}

}  // namespace device

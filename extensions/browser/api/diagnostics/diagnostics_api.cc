// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/diagnostics/diagnostics_api.h"

namespace {

const char kErrorPingNotImplemented[] = "Not implemented";
const char kErrorPingFailed[] = "Failed to send ping packet";

}  // namespace

namespace extensions {

namespace SendPacket = api::diagnostics::SendPacket;

DiagnosticsSendPacketFunction::DiagnosticsSendPacketFunction() = default;
DiagnosticsSendPacketFunction::~DiagnosticsSendPacketFunction() = default;

void DiagnosticsSendPacketFunction::OnCompleted(
    SendPacketResultCode result_code,
    const std::string& ip,
    double latency) {
  switch (result_code) {
    case SEND_PACKET_OK: {
      api::diagnostics::SendPacketResult result;
      result.ip = ip;
      result.latency = latency;
      Respond(OneArgument(SendPacket::Results::Create(result)));
      break;
    }
    case SEND_PACKET_NOT_IMPLEMENTED:
      Respond(Error(kErrorPingNotImplemented));
      break;
    case SEND_PACKET_FAILED:
      Respond(Error(kErrorPingFailed));
      break;
  }
}

}  // namespace extensions

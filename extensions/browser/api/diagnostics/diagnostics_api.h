// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DIAGNOSTICS_DIAGNOSTICS_API_H_
#define EXTENSIONS_BROWSER_API_DIAGNOSTICS_DIAGNOSTICS_API_H_

#include <memory>
#include <string>

#include "extensions/browser/extension_function.h"
#include "extensions/common/api/diagnostics.h"

namespace extensions {

class DiagnosticsSendPacketFunction : public UIThreadExtensionFunction {
 public:
  // Result code for sending packet. Platform specific AsyncWorkStart() will
  // finish with this ResultCode so we can maximize shared code.
  enum SendPacketResultCode {
    // Ping packed is sent and ICMP reply is received before time out.
    SEND_PACKET_OK,

    // Not implemented on the platform.
    SEND_PACKET_NOT_IMPLEMENTED,

    // The ping operation failed because of timeout or network unreachable.
    SEND_PACKET_FAILED,
  };

  DECLARE_EXTENSION_FUNCTION("diagnostics.sendPacket", DIAGNOSTICS_SENDPACKET)

  DiagnosticsSendPacketFunction();

 protected:
  ~DiagnosticsSendPacketFunction() override;

  // UIThreadExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnCompleted(SendPacketResultCode result_code,
                   const std::string& ip,
                   double latency);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DIAGNOSTICS_DIAGNOSTICS_API_H_

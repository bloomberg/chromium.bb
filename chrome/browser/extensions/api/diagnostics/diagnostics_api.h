// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAGNOSTICS_DIAGNOSTICS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAGNOSTICS_DIAGNOSTICS_API_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/diagnostics.h"
#include "extensions/browser/api/async_api_function.h"

namespace extensions {

class DiagnosticsSendPacketFunction : public AsyncApiFunction {
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

  DECLARE_EXTENSION_FUNCTION("diagnostics.sendPacket",
                             DIAGNOSTICS_SENDPACKET);

  DiagnosticsSendPacketFunction();

 protected:
  virtual ~DiagnosticsSendPacketFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  // This methods will be implemented differently on different platforms.
  virtual void AsyncWorkStart() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  void SendPingPacket();
  void OnCompleted(SendPacketResultCode result_code,
                   const std::string& ip,
                   double latency);

  scoped_ptr<extensions::api::diagnostics::SendPacket::Params>
      parameters_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAGNOSTICS_DIAGNOSTICS_API_H_

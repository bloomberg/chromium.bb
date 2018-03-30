// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_type_converters.h"

#include "base/logging.h"

namespace mojo {

// static
::device::U2fTransportProtocol
TypeConverter<::device::U2fTransportProtocol,
              ::webauth::mojom::AuthenticatorTransport>::
    Convert(const ::webauth::mojom::AuthenticatorTransport& input) {
  switch (input) {
    case ::webauth::mojom::AuthenticatorTransport::USB:
      return ::device::U2fTransportProtocol::kUsbHumanInterfaceDevice;
    case ::webauth::mojom::AuthenticatorTransport::NFC:
      return ::device::U2fTransportProtocol::kNearFieldCommunication;
    case ::webauth::mojom::AuthenticatorTransport::BLE:
      return ::device::U2fTransportProtocol::kBluetoothLowEnergy;
  }
  NOTREACHED();
  return ::device::U2fTransportProtocol::kUsbHumanInterfaceDevice;
}

}  // namespace mojo

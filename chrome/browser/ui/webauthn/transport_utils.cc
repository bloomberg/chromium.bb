// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_utils.h"

#include "base/logging.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

base::string16 GetTransportHumanReadableName(AuthenticatorTransport transport) {
  int message_id = 0;
  switch (transport) {
    case AuthenticatorTransport::kBluetoothLowEnergy:
      message_id = IDS_WEBAUTHN_TRANSPORT_BLE;
      break;
    case AuthenticatorTransport::kNearFieldCommunication:
      message_id = IDS_WEBAUTHN_TRANSPORT_NFC;
      break;
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      message_id = IDS_WEBAUTHN_TRANSPORT_USB;
      break;
    case AuthenticatorTransport::kInternal:
      message_id = IDS_WEBAUTHN_TRANSPORT_INTERNAL;
      break;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      message_id = IDS_WEBAUTHN_TRANSPORT_CABLE;
      break;
  }
  DCHECK_NE(message_id, 0);
  return l10n_util::GetStringUTF16(message_id);
}

const gfx::VectorIcon& GetTransportVectorIcon(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kBluetoothLowEnergy:
      return kBluetoothIcon;
    case AuthenticatorTransport::kNearFieldCommunication:
      return kNfcIcon;
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return vector_icons::kUsbIcon;
    case AuthenticatorTransport::kInternal:
      return kFingerprintIcon;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      return kSmartphoneIcon;
  }
  NOTREACHED();
  return kFingerprintIcon;
}

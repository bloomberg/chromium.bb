// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TPM_FIRMWARE_UPDATE_H_
#define CHROME_BROWSER_CHROMEOS_TPM_FIRMWARE_UPDATE_H_

#include <memory>

#include "base/callback_forward.h"

namespace base {
class DictionaryValue;
}

namespace enterprise_management {
class TPMFirmwareUpdateSettingsProto;
}

namespace chromeos {
namespace tpm_firmware_update {

// Decodes the TPM firmware update settings into base::Value representation.
std::unique_ptr<base::DictionaryValue> DecodeSettingsProto(
    const enterprise_management::TPMFirmwareUpdateSettingsProto& settings);

// Check whether the update should be offered as part of the powerwash flow.
void ShouldOfferUpdateViaPowerwash(base::OnceCallback<void(bool)> completion);

}  // namespace tpm_firmware_update
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_TPM_FIRMWARE_UPDATE_H_

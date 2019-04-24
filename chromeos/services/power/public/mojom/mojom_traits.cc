// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/power/public/mojom/mojom_traits.h"

namespace mojo {

// static
chromeos::power::mojom::BacklightBrightnessChangeCause
EnumTraits<chromeos::power::mojom::BacklightBrightnessChangeCause,
           power_manager::BacklightBrightnessChange_Cause>::
    ToMojom(power_manager::BacklightBrightnessChange_Cause cause) {
  switch (cause) {
    case power_manager::BacklightBrightnessChange_Cause_USER_REQUEST:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::
          kUserRequest;
    case power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::
          kUserActivity;
    case power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::
          kUserInactivity;
    case power_manager::BacklightBrightnessChange_Cause_AMBIENT_LIGHT_CHANGED:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::
          kAmbientLightChanged;
    case power_manager::
        BacklightBrightnessChange_Cause_EXTERNAL_POWER_CONNECTED:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::
          kExternalPowerConnected;
    case power_manager::
        BacklightBrightnessChange_Cause_EXTERNAL_POWER_DISCONNECTED:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::
          kExternalPowerDisconnected;
    case power_manager::BacklightBrightnessChange_Cause_FORCED_OFF:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::kForcedOff;
    case power_manager::BacklightBrightnessChange_Cause_NO_LONGER_FORCED_OFF:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::
          kNoLongerForcedOff;
    case power_manager::BacklightBrightnessChange_Cause_OTHER:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::kOther;
    case power_manager::BacklightBrightnessChange_Cause_MODEL:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::kModel;
    case power_manager::BacklightBrightnessChange_Cause_WAKE_NOTIFICATION:
      return chromeos::power::mojom::BacklightBrightnessChangeCause::
          kWakeNotification;
  }
  NOTREACHED();
  return chromeos::power::mojom::BacklightBrightnessChangeCause::kUserRequest;
}

// static
bool EnumTraits<chromeos::power::mojom::BacklightBrightnessChangeCause,
                power_manager::BacklightBrightnessChange_Cause>::
    FromMojom(chromeos::power::mojom::BacklightBrightnessChangeCause cause,
              power_manager::BacklightBrightnessChange_Cause* out) {
  switch (cause) {
    case chromeos::power::mojom::BacklightBrightnessChangeCause::kUserRequest:
      *out = power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::kUserActivity:
      *out = power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::
        kUserInactivity:
      *out = power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::
        kAmbientLightChanged:
      *out =
          power_manager::BacklightBrightnessChange_Cause_AMBIENT_LIGHT_CHANGED;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::
        kExternalPowerConnected:
      *out = power_manager::
          BacklightBrightnessChange_Cause_EXTERNAL_POWER_CONNECTED;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::
        kExternalPowerDisconnected:
      *out = power_manager::
          BacklightBrightnessChange_Cause_EXTERNAL_POWER_DISCONNECTED;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::kForcedOff:
      *out = power_manager::BacklightBrightnessChange_Cause_FORCED_OFF;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::
        kNoLongerForcedOff:
      *out =
          power_manager::BacklightBrightnessChange_Cause_NO_LONGER_FORCED_OFF;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::kOther:
      *out = power_manager::BacklightBrightnessChange_Cause_OTHER;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::kModel:
      *out = power_manager::BacklightBrightnessChange_Cause_MODEL;
      return true;
    case chromeos::power::mojom::BacklightBrightnessChangeCause::
        kWakeNotification:
      *out = power_manager::BacklightBrightnessChange_Cause_WAKE_NOTIFICATION;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<chromeos::power::mojom::BacklightBrightnessChangeDataView,
                  power_manager::BacklightBrightnessChange>::
    Read(chromeos::power::mojom::BacklightBrightnessChangeDataView data,
         power_manager::BacklightBrightnessChange* out) {
  if (data.percent() < 0. || data.percent() > 100.)
    return false;

  out->set_percent(data.percent());

  power_manager::BacklightBrightnessChange_Cause cause;
  if (!data.ReadCause(&cause))
    return false;

  out->set_cause(cause);
  return true;
}

}  // namespace mojo

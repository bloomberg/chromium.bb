// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/power/public/mojom/mojom_traits.h"

namespace mojo {

using chromeos::power::mojom::BacklightBrightnessChangeCause;
using chromeos::power::mojom::RequestRestartReason;
using chromeos::power::mojom::SetBacklightBrightnessRequestCause;
using chromeos::power::mojom::SetBacklightBrightnessRequestTransition;

// static
SetBacklightBrightnessRequestTransition
EnumTraits<SetBacklightBrightnessRequestTransition,
           power_manager::SetBacklightBrightnessRequest_Transition>::
    ToMojom(
        power_manager::SetBacklightBrightnessRequest_Transition transition) {
  switch (transition) {
    case power_manager::SetBacklightBrightnessRequest_Transition_GRADUAL:
      return SetBacklightBrightnessRequestTransition::kGradual;
    case power_manager::SetBacklightBrightnessRequest_Transition_INSTANT:
      return SetBacklightBrightnessRequestTransition::kInstant;
  }
  NOTREACHED();
  return SetBacklightBrightnessRequestTransition::kGradual;
}

// static
bool EnumTraits<SetBacklightBrightnessRequestTransition,
                power_manager::SetBacklightBrightnessRequest_Transition>::
    FromMojom(SetBacklightBrightnessRequestTransition transition,
              power_manager::SetBacklightBrightnessRequest_Transition* out) {
  switch (transition) {
    case SetBacklightBrightnessRequestTransition::kGradual:
      *out = power_manager::SetBacklightBrightnessRequest_Transition_GRADUAL;
      return true;
    case SetBacklightBrightnessRequestTransition::kInstant:
      *out = power_manager::SetBacklightBrightnessRequest_Transition_INSTANT;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
SetBacklightBrightnessRequestCause
EnumTraits<SetBacklightBrightnessRequestCause,
           power_manager::SetBacklightBrightnessRequest_Cause>::
    ToMojom(power_manager::SetBacklightBrightnessRequest_Cause cause) {
  switch (cause) {
    case power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST:
      return SetBacklightBrightnessRequestCause::kUserRequest;
    case power_manager::SetBacklightBrightnessRequest_Cause_MODEL:
      return SetBacklightBrightnessRequestCause::kModel;
  }
  NOTREACHED();
  return SetBacklightBrightnessRequestCause::kUserRequest;
}

// static
bool EnumTraits<SetBacklightBrightnessRequestCause,
                power_manager::SetBacklightBrightnessRequest_Cause>::
    FromMojom(SetBacklightBrightnessRequestCause cause,
              power_manager::SetBacklightBrightnessRequest_Cause* out) {
  switch (cause) {
    case SetBacklightBrightnessRequestCause::kUserRequest:
      *out = power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST;
      return true;
    case SetBacklightBrightnessRequestCause::kModel:
      *out = power_manager::SetBacklightBrightnessRequest_Cause_MODEL;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<chromeos::power::mojom::SetBacklightBrightnessRequestDataView,
                  power_manager::SetBacklightBrightnessRequest>::
    Read(chromeos::power::mojom::SetBacklightBrightnessRequestDataView data,
         power_manager::SetBacklightBrightnessRequest* out) {
  if (data.percent() < 0. || data.percent() > 100.)
    return false;

  out->set_percent(data.percent());

  power_manager::SetBacklightBrightnessRequest_Transition transition;
  power_manager::SetBacklightBrightnessRequest_Cause cause;
  if (!data.ReadTransition(&transition) || !data.ReadCause(&cause))
    return false;

  out->set_transition(transition);
  out->set_cause(cause);
  return true;
}

// static
BacklightBrightnessChangeCause
EnumTraits<BacklightBrightnessChangeCause,
           power_manager::BacklightBrightnessChange_Cause>::
    ToMojom(power_manager::BacklightBrightnessChange_Cause cause) {
  switch (cause) {
    case power_manager::BacklightBrightnessChange_Cause_USER_REQUEST:
      return BacklightBrightnessChangeCause::kUserRequest;
    case power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY:
      return BacklightBrightnessChangeCause::kUserActivity;
    case power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY:
      return BacklightBrightnessChangeCause::kUserInactivity;
    case power_manager::BacklightBrightnessChange_Cause_AMBIENT_LIGHT_CHANGED:
      return BacklightBrightnessChangeCause::kAmbientLightChanged;
    case power_manager::
        BacklightBrightnessChange_Cause_EXTERNAL_POWER_CONNECTED:
      return BacklightBrightnessChangeCause::kExternalPowerConnected;
    case power_manager::
        BacklightBrightnessChange_Cause_EXTERNAL_POWER_DISCONNECTED:
      return BacklightBrightnessChangeCause::kExternalPowerDisconnected;
    case power_manager::BacklightBrightnessChange_Cause_FORCED_OFF:
      return BacklightBrightnessChangeCause::kForcedOff;
    case power_manager::BacklightBrightnessChange_Cause_NO_LONGER_FORCED_OFF:
      return BacklightBrightnessChangeCause::kNoLongerForcedOff;
    case power_manager::BacklightBrightnessChange_Cause_OTHER:
      return BacklightBrightnessChangeCause::kOther;
    case power_manager::BacklightBrightnessChange_Cause_MODEL:
      return BacklightBrightnessChangeCause::kModel;
    case power_manager::BacklightBrightnessChange_Cause_WAKE_NOTIFICATION:
      return BacklightBrightnessChangeCause::kWakeNotification;
  }
  NOTREACHED();
  return BacklightBrightnessChangeCause::kUserRequest;
}

// static
bool EnumTraits<BacklightBrightnessChangeCause,
                power_manager::BacklightBrightnessChange_Cause>::
    FromMojom(BacklightBrightnessChangeCause cause,
              power_manager::BacklightBrightnessChange_Cause* out) {
  switch (cause) {
    case BacklightBrightnessChangeCause::kUserRequest:
      *out = power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
      return true;
    case BacklightBrightnessChangeCause::kUserActivity:
      *out = power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY;
      return true;
    case BacklightBrightnessChangeCause::kUserInactivity:
      *out = power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY;
      return true;
    case BacklightBrightnessChangeCause::kAmbientLightChanged:
      *out =
          power_manager::BacklightBrightnessChange_Cause_AMBIENT_LIGHT_CHANGED;
      return true;
    case BacklightBrightnessChangeCause::kExternalPowerConnected:
      *out = power_manager::
          BacklightBrightnessChange_Cause_EXTERNAL_POWER_CONNECTED;
      return true;
    case BacklightBrightnessChangeCause::kExternalPowerDisconnected:
      *out = power_manager::
          BacklightBrightnessChange_Cause_EXTERNAL_POWER_DISCONNECTED;
      return true;
    case BacklightBrightnessChangeCause::kForcedOff:
      *out = power_manager::BacklightBrightnessChange_Cause_FORCED_OFF;
      return true;
    case BacklightBrightnessChangeCause::kNoLongerForcedOff:
      *out =
          power_manager::BacklightBrightnessChange_Cause_NO_LONGER_FORCED_OFF;
      return true;
    case BacklightBrightnessChangeCause::kOther:
      *out = power_manager::BacklightBrightnessChange_Cause_OTHER;
      return true;
    case BacklightBrightnessChangeCause::kModel:
      *out = power_manager::BacklightBrightnessChange_Cause_MODEL;
      return true;
    case BacklightBrightnessChangeCause::kWakeNotification:
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

// static
RequestRestartReason
EnumTraits<RequestRestartReason, power_manager::RequestRestartReason>::ToMojom(
    power_manager::RequestRestartReason reason) {
  switch (reason) {
    case power_manager::REQUEST_RESTART_FOR_USER:
      return RequestRestartReason::kForUser;
    case power_manager::REQUEST_RESTART_FOR_UPDATE:
      return RequestRestartReason::kForUpdate;
    case power_manager::REQUEST_RESTART_OTHER:
      return RequestRestartReason::kOther;
  }
}

// static
bool EnumTraits<RequestRestartReason, power_manager::RequestRestartReason>::
    FromMojom(chromeos::power::mojom::RequestRestartReason reason,
              power_manager::RequestRestartReason* out) {
  switch (reason) {
    case RequestRestartReason::kForUser:
      *out = power_manager::REQUEST_RESTART_FOR_USER;
      return true;
    case RequestRestartReason::kForUpdate:
      *out = power_manager::REQUEST_RESTART_FOR_UPDATE;
      return true;
    case RequestRestartReason::kOther:
      *out = power_manager::REQUEST_RESTART_OTHER;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo

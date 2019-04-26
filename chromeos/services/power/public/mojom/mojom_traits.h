// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_POWER_PUBLIC_MOJOM_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_POWER_PUBLIC_MOJOM_MOJOM_TRAITS_H_

#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/services/power/public/mojom/power_manager.mojom-shared.h"
#include "chromeos/services/power/public/mojom/power_manager.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"

namespace mojo {

template <>
struct EnumTraits<
    chromeos::power::mojom::SetBacklightBrightnessRequestTransition,
    power_manager::SetBacklightBrightnessRequest_Transition> {
  static chromeos::power::mojom::SetBacklightBrightnessRequestTransition
  ToMojom(power_manager::SetBacklightBrightnessRequest_Transition cause);

  static bool FromMojom(
      chromeos::power::mojom::SetBacklightBrightnessRequestTransition cause,
      power_manager::SetBacklightBrightnessRequest_Transition* out);
};

template <>
struct EnumTraits<chromeos::power::mojom::SetBacklightBrightnessRequestCause,
                  power_manager::SetBacklightBrightnessRequest_Cause> {
  static chromeos::power::mojom::SetBacklightBrightnessRequestCause ToMojom(
      power_manager::SetBacklightBrightnessRequest_Cause cause);

  static bool FromMojom(
      chromeos::power::mojom::SetBacklightBrightnessRequestCause cause,
      power_manager::SetBacklightBrightnessRequest_Cause* out);
};

template <>
struct StructTraits<
    chromeos::power::mojom::SetBacklightBrightnessRequestDataView,
    power_manager::SetBacklightBrightnessRequest> {
  static double percent(
      const power_manager::SetBacklightBrightnessRequest& request) {
    return request.percent();
  }
  static power_manager::SetBacklightBrightnessRequest_Transition transition(
      const power_manager::SetBacklightBrightnessRequest& request) {
    return request.transition();
  }
  static power_manager::SetBacklightBrightnessRequest_Cause cause(
      const power_manager::SetBacklightBrightnessRequest& request) {
    return request.cause();
  }
  static bool Read(
      chromeos::power::mojom::SetBacklightBrightnessRequestDataView data,
      power_manager::SetBacklightBrightnessRequest* out);
};

template <>
struct EnumTraits<chromeos::power::mojom::BacklightBrightnessChangeCause,
                  power_manager::BacklightBrightnessChange_Cause> {
  static chromeos::power::mojom::BacklightBrightnessChangeCause ToMojom(
      power_manager::BacklightBrightnessChange_Cause cause);

  static bool FromMojom(
      chromeos::power::mojom::BacklightBrightnessChangeCause cause,
      power_manager::BacklightBrightnessChange_Cause* out);
};

template <>
struct StructTraits<chromeos::power::mojom::BacklightBrightnessChangeDataView,
                    power_manager::BacklightBrightnessChange> {
  static double percent(
      const power_manager::BacklightBrightnessChange& change) {
    return change.percent();
  }
  static power_manager::BacklightBrightnessChange_Cause cause(
      const power_manager::BacklightBrightnessChange& change) {
    return change.cause();
  }
  static bool Read(
      chromeos::power::mojom::BacklightBrightnessChangeDataView data,
      power_manager::BacklightBrightnessChange* out);
};

template <>
struct EnumTraits<chromeos::power::mojom::RequestRestartReason,
                  power_manager::RequestRestartReason> {
  static chromeos::power::mojom::RequestRestartReason ToMojom(
      power_manager::RequestRestartReason reason);

  static bool FromMojom(chromeos::power::mojom::RequestRestartReason reason,
                        power_manager::RequestRestartReason* out);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_POWER_PUBLIC_MOJOM_MOJOM_TRAITS_H_

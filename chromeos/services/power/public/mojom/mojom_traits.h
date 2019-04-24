// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_POWER_PUBLIC_MOJOM_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_POWER_PUBLIC_MOJOM_MOJOM_TRAITS_H_

#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/services/power/public/mojom/power_manager.mojom-shared.h"
#include "chromeos/services/power/public/mojom/power_manager.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

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

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_POWER_PUBLIC_MOJOM_MOJOM_TRAITS_H_

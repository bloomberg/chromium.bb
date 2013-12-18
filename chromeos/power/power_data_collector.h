// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POWER_POWER_DATA_COLLECTOR_H_
#define CHROMEOS_POWER_POWER_DATA_COLLECTOR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/power_manager_client.h"

namespace power_manager {
class PowerSupplyProperties;
}

namespace chromeos {

// A class which starts collecting power metrics, like the battery charge, as
// soon as it is initialized via Initialize().
//
// This class is implemented as a global singleton, initialized after
// DBusThreadManager which it depends on.
class CHROMEOS_EXPORT PowerDataCollector : public PowerManagerClient::Observer {
 public:
  struct PowerSupplySnapshot {
    PowerSupplySnapshot();

    // Time when the snapshot was captured.
    base::TimeTicks time;

    // True if connected to external power at the time of the snapshot.
    bool external_power;

    // The battery charge as a percentage of full charge in range [0.0, 100.00].
    double battery_percent;
  };

  const std::vector<PowerSupplySnapshot>& power_supply_data() const {
    return power_supply_data_;
  }

  // Can be called only after DBusThreadManager is initialized.
  static void Initialize();

  // Can be called only if initialized via Initialize, and before
  // DBusThreadManager is destroyed.
  static void Shutdown();

  // Returns the global instance of PowerDataCollector.
  static PowerDataCollector* Get();

  // PowerManagerClient::Observer implementation:
  virtual void PowerChanged(
      const power_manager::PowerSupplyProperties& prop) OVERRIDE;

 private:
  PowerDataCollector();

  virtual ~PowerDataCollector();

  std::vector<PowerSupplySnapshot> power_supply_data_;

  DISALLOW_COPY_AND_ASSIGN(PowerDataCollector);
};

}  // namespace chromeos

#endif  // CHROMEOS_POWER_POWER_DATA_COLLECTOR_H_

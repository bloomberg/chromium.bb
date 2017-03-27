// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// PowerStatus is a singleton that receives updates about the system's
// power status from chromeos::PowerManagerClient and makes the information
// available to interested classes within Ash.
class ASH_EXPORT PowerStatus : public chromeos::PowerManagerClient::Observer {
 public:
  // Types of badges which can be drawn on top of a battery icon.
  enum IconBadge {
    ICON_BADGE_NONE,
    ICON_BADGE_ALERT,
    ICON_BADGE_BOLT,
    ICON_BADGE_X,
    ICON_BADGE_UNRELIABLE
  };

  // Different styles of battery icons.
  enum IconSet { ICON_LIGHT, ICON_DARK };

  // Interface for classes that wish to be notified when the power status
  // has changed.
  class Observer {
   public:
    // Called when the power status changes.
    virtual void OnPowerStatusChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Power source types.
  enum DeviceType {
    // Dedicated charger (AC adapter, USB power supply, etc.).
    DEDICATED_CHARGER,

    // Dual-role device.
    DUAL_ROLE_USB,
  };

  // Information about an available power source.
  struct PowerSource {
    // ID provided by kernel.
    std::string id;

    // Type of power source.
    DeviceType type;

    // Message ID of a description for this port.
    int description_id;
  };

  // Information about the battery image corresponding to the status at a given
  // point in time. This can be cached and later compared to avoid unnecessarily
  // updating onscreen icons (GetBatteryImage() creates a new image on each
  // call).
  struct BatteryImageInfo {
    BatteryImageInfo()
        : resource_id(-1),
          offset(-1),
          index(-1),
          icon_badge(ICON_BADGE_NONE),
          charge_level(-1) {}

    bool operator==(const BatteryImageInfo& o) const;

    bool operator!=(const BatteryImageInfo& o) const { return !(*this == o); }

    // Resource ID of the image containing the specific battery icon to use.
    // Only used in non-MD.
    int resource_id;

    // Horizontal offset in the battery icon array image. The USB / "unreliable
    // charging" image has a single column of icons; the other image contains a
    // "battery" column on the left and a "line power" column on the right.
    // Only used in non-MD.
    int offset;

    // Vertical offset corresponding to the current battery level. Only used in
    // non-MD.
    int index;

    // The badge (lightning bolt, exclamation mark, etc) that should be drawn
    // on top of the battery icon. Only used for MD.
    // TODO(tdanderson): Consider using gfx::VectorIconId instead of this enum
    // once all possible badges have been vectorized. See crbug.com/617298.
    IconBadge icon_badge;

    // A value between 0 and kBatteryImageHeightMD representing the height
    // of the battery's charge level in dp. Only used for MD.
    int charge_level;
  };

  // Maximum battery time-to-full or time-to-empty that should be displayed
  // in the UI. If the current is close to zero, battery time estimates can
  // get very large; avoid displaying these large numbers.
  static const int kMaxBatteryTimeToDisplaySec;

  // An alert badge is drawn over the material design battery icon if the
  // battery is not connected to a charger and has less than
  // |kCriticalBatteryChargePercentageMd| percentage of charge remaining.
  static const double kCriticalBatteryChargePercentageMd;

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static PowerStatus* Get();

  // Returns true if |time|, a time returned by GetBatteryTimeToEmpty() or
  // GetBatteryTimeToFull(), should be displayed in the UI.
  // Less-than-a-minute or very large values aren't displayed.
  static bool ShouldDisplayBatteryTime(const base::TimeDelta& time);

  // Copies the hour and minute components of |time| to |hours| and |minutes|.
  // The minute component is rounded rather than truncated: a |time| value
  // corresponding to 92 seconds will produce a |minutes| value of 2, for
  // example.
  static void SplitTimeIntoHoursAndMinutes(const base::TimeDelta& time,
                                           int* hours,
                                           int* minutes);

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Requests updated status from the power manager.
  void RequestStatusUpdate();

  // Changes the power source to the source with the given ID.
  void SetPowerSource(const std::string& id);

  // Returns true if a battery is present.
  bool IsBatteryPresent() const;

  // Returns true if the battery is full. This also implies that a charger
  // is connected.
  bool IsBatteryFull() const;

  // Returns true if the battery is charging. Note that this implies that a
  // charger is connected but the converse is not necessarily true: the
  // battery may be discharging even while a (perhaps low-power) charger is
  // connected. Use Is*Connected() to test for the presence of a charger
  // and also see IsBatteryDischargingOnLinePower().
  bool IsBatteryCharging() const;

  // Returns true if the battery is discharging (or neither charging nor
  // discharging while not being full) while line power is connected.
  bool IsBatteryDischargingOnLinePower() const;

  // Returns the battery's remaining charge as a value in the range [0.0,
  // 100.0].
  double GetBatteryPercent() const;

  // Returns the battery's remaining charge, rounded to an integer with a
  // maximum value of 100.
  int GetRoundedBatteryPercent() const;

  // Returns true if the battery's time-to-full and time-to-empty estimates
  // should not be displayed because the power manager is still calculating
  // them.
  bool IsBatteryTimeBeingCalculated() const;

  // Returns the estimated time until the battery is empty (if line power
  // is disconnected) or full (if line power is connected). These estimates
  // should only be used if IsBatteryTimeBeingCalculated() returns false.
  base::TimeDelta GetBatteryTimeToEmpty() const;
  base::TimeDelta GetBatteryTimeToFull() const;

  // Returns true if line power (including a charger of any type) is connected.
  bool IsLinePowerConnected() const;

  // Returns true if an official, non-USB charger is connected.
  bool IsMainsChargerConnected() const;

  // Returns true if a USB charger (which is likely to only support a low
  // charging rate) is connected.
  bool IsUsbChargerConnected() const;

  // Returns true if the system allows some connected devices to function as
  // either power sources or sinks.
  bool SupportsDualRoleDevices() const;

  // Returns true if at least one dual-role device is connected.
  bool HasDualRoleDevices() const;

  // Returns a list of available power sources which the user may select.
  std::vector<PowerSource> GetPowerSources() const;

  // Returns the ID of the currently used power source, or an empty string if no
  // power source is selected.
  std::string GetCurrentPowerSourceID() const;

  // Returns information about the image that would be returned by
  // GetBatteryImage(). This can be cached and compared against future objects
  // returned by this method to avoid creating new images unnecessarily.
  BatteryImageInfo GetBatteryImageInfo(IconSet icon_set) const;

  // A helper function called by GetBatteryImageInfo(). Populates the
  // MD-specific fields of |info|.
  void CalculateBatteryImageInfoMd(BatteryImageInfo* info) const;

  // A helper function called by GetBatteryImageInfo(). Populates the
  // non-MD-specific fields of |info|.
  void CalculateBatteryImageInfoNonMd(BatteryImageInfo* info,
                                      const IconSet& icon_set) const;

  // Creates a new image that should be shown for the battery's current state.
  gfx::ImageSkia GetBatteryImage(const BatteryImageInfo& info) const;

  // A version of GetBatteryImage() that is used when material design is not
  // enabled.
  // TODO(tdanderson): Remove this once material design is enabled by default.
  // See crbug.com/614453.
  gfx::ImageSkia GetBatteryImageNonMd(const BatteryImageInfo& info) const;

  // Returns an string describing the current state for accessibility.
  base::string16 GetAccessibleNameString(bool full_description) const;

  // Updates |proto_|. Does not notify observers.
  void SetProtoForTesting(const power_manager::PowerSupplyProperties& proto);

 protected:
  PowerStatus();
  ~PowerStatus() override;

 private:
  // Overriden from PowerManagerClient::Observer.
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  base::ObserverList<Observer> observers_;

  // Current state.
  power_manager::PowerSupplyProperties proto_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatus);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_

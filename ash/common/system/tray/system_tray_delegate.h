// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#define ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/login_status.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string16.h"

namespace base {
class TimeDelta;
class TimeTicks;
}

namespace device {
enum class BluetoothDeviceType;
}

namespace ash {
struct IMEInfo;
struct IMEPropertyInfo;

class CustodianInfoTrayObserver;
class SystemTray;
class SystemTrayItem;

using IMEInfoList = std::vector<IMEInfo>;
using IMEPropertyInfoList = std::vector<IMEPropertyInfo>;

struct ASH_EXPORT BluetoothDeviceInfo {
  BluetoothDeviceInfo();
  BluetoothDeviceInfo(const BluetoothDeviceInfo& other);
  ~BluetoothDeviceInfo();

  std::string address;
  base::string16 display_name;
  bool connected;
  bool connecting;
  bool paired;
  device::BluetoothDeviceType device_type;
};

using BluetoothDeviceList = std::vector<BluetoothDeviceInfo>;

class NetworkingConfigDelegate;

// SystemTrayDelegate is intended for delegating tasks in the System Tray to the
// application (e.g. Chrome). These tasks should be limited to application
// (browser) specific tasks. For non application specific tasks, where possible,
// components/, chromeos/, device/, etc., code should be used directly. If more
// than one related method is being added, consider adding an additional
// specific delegate (e.g. CastConfigDelegate).
//
// These methods should all have trivial default implementations for platforms
// that do not implement the method (e.g. return false or nullptr). This
// eliminates the need to propagate default implementations across the various
// implementations of this class. Consumers of this delegate should handle the
// default return value (e.g. nullptr).
class ASH_EXPORT SystemTrayDelegate {
 public:
  SystemTrayDelegate();
  virtual ~SystemTrayDelegate();

  // Called after SystemTray has been instantiated.
  virtual void Initialize();

  // Gets information about the active user.
  virtual LoginStatus GetUserLoginStatus() const;

  // Returns the domain that manages the device, if it is enterprise-enrolled.
  virtual std::string GetEnterpriseDomain() const;

  // Returns the realm that manages the device, if it is enterprise enrolled
  // with Active Directory and joined the realm (Active Directory domain).
  virtual std::string GetEnterpriseRealm() const;

  // Returns notification for enterprise enrolled devices.
  virtual base::string16 GetEnterpriseMessage() const;

  // Returns the display email of the user that manages the current supervised
  // user.
  virtual std::string GetSupervisedUserManager() const;

  // Returns the name of the user that manages the current supervised user.
  virtual base::string16 GetSupervisedUserManagerName() const;

  // Returns the notification for supervised users.
  virtual base::string16 GetSupervisedUserMessage() const;

  // Returns true if the current user is supervised: has legacy supervised
  // account or kid account.
  virtual bool IsUserSupervised() const;

  // Returns true if the current user is child.
  // TODO(merkulova): remove on FakeUserManager componentization.
  // crbug.com/443119
  virtual bool IsUserChild() const;

  // Returns true if settings menu item should appear.
  virtual bool ShouldShowSettings() const;

  // Returns true if notification tray should appear.
  virtual bool ShouldShowNotificationTray() const;

  // Shows information about enterprise enrolled devices.
  virtual void ShowEnterpriseInfo();

  // Shows login UI to add other users to this session.
  virtual void ShowUserLogin();

  // Returns a list of available bluetooth devices.
  virtual void GetAvailableBluetoothDevices(BluetoothDeviceList* devices);

  // Requests bluetooth start discovering devices.
  virtual void BluetoothStartDiscovering();

  // Requests bluetooth stop discovering devices.
  virtual void BluetoothStopDiscovering();

  // Connect to a specific bluetooth device.
  virtual void ConnectToBluetoothDevice(const std::string& address);

  // Returns true if bluetooth adapter is discovering bluetooth devices.
  virtual bool IsBluetoothDiscovering() const;

  // Returns the currently selected IME.
  virtual void GetCurrentIME(IMEInfo* info);

  // Returns a list of availble IMEs.
  virtual void GetAvailableIMEList(IMEInfoList* list);

  // Returns a list of properties for the currently selected IME.
  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list);

  // Returns a non-empty string if IMEs are managed by policy.
  virtual base::string16 GetIMEManagedMessage();

  // Switches to the selected input method.
  virtual void SwitchIME(const std::string& ime_id);

  // Activates an IME property.
  virtual void ActivateIMEProperty(const std::string& key);

  // Shows UI to manage bluetooth devices.
  virtual void ManageBluetoothDevices();

  // Toggles bluetooth.
  virtual void ToggleBluetooth();

  // Returns whether bluetooth capability is available.
  virtual bool GetBluetoothAvailable();

  // Returns whether bluetooth is enabled.
  virtual bool GetBluetoothEnabled();

  // Returns whether the delegate has initiated a bluetooth discovery session.
  virtual bool GetBluetoothDiscovering();

  // Returns NetworkingConfigDelegate. May return nullptr.
  virtual NetworkingConfigDelegate* GetNetworkingConfigDelegate() const;

  // Retrieves the session start time. Returns |false| if the time is not set.
  virtual bool GetSessionStartTime(base::TimeTicks* session_start_time);

  // Retrieves the session length limit. Returns |false| if no limit is set.
  virtual bool GetSessionLengthLimit(base::TimeDelta* session_length_limit);

  // Get the system tray menu size in pixels (dependent on the language).
  // This is not used in material design and should be removed during pre-MD
  // code cleanup. See https://crbug.com/614453.
  virtual int GetSystemTrayMenuWidth();

  // The active user has been changed. This will be called when the UI is ready
  // to be switched to the new user.
  // Note: This will happen after SessionStateObserver::ActiveUserChanged fires.
  virtual void ActiveUserWasChanged();

  // Returns true when the Search key is configured to be treated as Caps Lock.
  virtual bool IsSearchKeyMappedToCapsLock();

  // Adding observers that are notified when supervised info is being changed.
  virtual void AddCustodianInfoTrayObserver(
      CustodianInfoTrayObserver* observer);

  virtual void RemoveCustodianInfoTrayObserver(
      CustodianInfoTrayObserver* observer);

  // Creates a system tray item for display rotation lock.
  // TODO(jamescook): Remove this when mus has support for display management
  // and we have a DisplayManager equivalent. See http://crbug.com/548429
  virtual std::unique_ptr<SystemTrayItem> CreateRotationLockTrayItem(
      SystemTray* tray);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_

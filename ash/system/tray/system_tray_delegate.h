// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/user/login_status.h"
#include "ash/volume_control_delegate.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class TimeDelta;
class TimeTicks;
}

namespace ash {

class CustodianInfoTrayObserver;
class ShutdownPolicyObserver;

struct ASH_EXPORT NetworkIconInfo {
  NetworkIconInfo();
  ~NetworkIconInfo();

  bool highlight() const { return connected || connecting; }

  bool connecting;
  bool connected;
  bool tray_icon_visible;
  gfx::ImageSkia image;
  base::string16 name;
  base::string16 description;
  std::string service_path;
  bool is_cellular;
};

struct ASH_EXPORT BluetoothDeviceInfo {
  BluetoothDeviceInfo();
  ~BluetoothDeviceInfo();

  std::string address;
  base::string16 display_name;
  bool connected;
  bool connecting;
  bool paired;
};

using BluetoothDeviceList = std::vector<BluetoothDeviceInfo>;

struct ASH_EXPORT IMEPropertyInfo {
  IMEPropertyInfo();
  ~IMEPropertyInfo();

  bool selected;
  std::string key;
  base::string16 name;
};

using IMEPropertyInfoList = std::vector<IMEPropertyInfo>;

struct ASH_EXPORT IMEInfo {
  IMEInfo();
  ~IMEInfo();

  bool selected;
  bool third_party;
  std::string id;
  base::string16 name;
  base::string16 medium_name;
  base::string16 short_name;
};

struct ASH_EXPORT UpdateInfo {
  enum UpdateSeverity {
    UPDATE_NORMAL,
    UPDATE_LOW_GREEN,
    UPDATE_HIGH_ORANGE,
    UPDATE_SEVERE_RED,
  };

  UpdateInfo();
  ~UpdateInfo();

  UpdateSeverity severity;
  bool update_required;
  bool factory_reset_required;
};

using IMEInfoList = std::vector<IMEInfo>;

class NetworkingConfigDelegate;
class VPNDelegate;

using RebootOnShutdownCallback = base::Callback<void(bool)>;

namespace tray {
class UserAccountsDelegate;
}  // namespace tray

// SystemTrayDelegate is intended for delegating tasks in the System Tray to the
// application (e.g. Chrome). These tasks should be limited to application
// (browser) specific tasks. For non application specific tasks, where possible,
// components/, chromeos/, device/, etc., code should be used directly. If more
// than one related method is being added, consider adding an additional
// specific delegate (e.g. VolumeControlDelegate).
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

  // Called before SystemTray is destroyed.
  virtual void Shutdown();

  // Returns true if system tray should be visible on startup.
  virtual bool GetTrayVisibilityOnStartup();

  // Gets information about the active user.
  virtual user::LoginStatus GetUserLoginStatus() const;

  // Shows UI for changing user's profile picture.
  virtual void ChangeProfilePicture();

  // Returns the domain that manages the device, if it is enterprise-enrolled.
  virtual const std::string GetEnterpriseDomain() const;

  // Returns notification for enterprise enrolled devices.
  virtual const base::string16 GetEnterpriseMessage() const;

  // Returns the display email of the user that manages the current supervised
  // user.
  virtual const std::string GetSupervisedUserManager() const;

  // Returns the name of the user that manages the current supervised user.
  virtual const base::string16 GetSupervisedUserManagerName() const;

  // Returns the notification for supervised users.
  virtual const base::string16 GetSupervisedUserMessage() const;

  // Returns true if the current user is supervised: has legacy supervised
  // account or kid account.
  virtual bool IsUserSupervised() const;

  // Returns true if the current user is child.
  // TODO(merkulova): remove on FakeUserManager componentization.
  // crbug.com/443119
  virtual bool IsUserChild() const;

  // Fills |info| structure (which must not be null) with current update info.
  virtual void GetSystemUpdateInfo(UpdateInfo* info) const;

  // Returns the desired hour clock type.
  virtual base::HourClockType GetHourClockType() const;

  // Shows settings.
  virtual void ShowSettings();

  // Returns true if settings menu item should appear.
  virtual bool ShouldShowSettings();

  // Shows the settings related to date, timezone etc.
  virtual void ShowDateSettings();

  // Shows the dialog to set system time, date, and timezone.
  virtual void ShowSetTimeDialog();

  // Shows the settings related to network. If |guid| is not empty,
  // show the settings for the corresponding network.
  virtual void ShowNetworkSettingsForGuid(const std::string& guid);

  // Shows the settings related to bluetooth.
  virtual void ShowBluetoothSettings();

  // Shows settings related to multiple displays.
  virtual void ShowDisplaySettings();

  // Shows the page that lets you disable performance tracing.
  virtual void ShowChromeSlow();

  // Returns true if the notification for the display configuration change
  // should appear.
  virtual bool ShouldShowDisplayNotification();

  // Shows settings related to input methods.
  virtual void ShowIMESettings();

  // Shows help.
  virtual void ShowHelp();

  // Show accessilibity help.
  virtual void ShowAccessibilityHelp();

  // Show the settings related to accessilibity.
  virtual void ShowAccessibilitySettings();

  // Shows more information about public account mode.
  virtual void ShowPublicAccountInfo();

  // Shows information about enterprise enrolled devices.
  virtual void ShowEnterpriseInfo();

  // Shows information about supervised users.
  virtual void ShowSupervisedUserInfo();

  // Shows login UI to add other users to this session.
  virtual void ShowUserLogin();

  // Attempts to sign out the user.
  virtual void SignOut();

  // Attempts to lock the screen.
  virtual void RequestLockScreen();

  // Attempts to restart the system for update.
  virtual void RequestRestartForUpdate();

  // Returns a list of available bluetooth devices.
  virtual void GetAvailableBluetoothDevices(BluetoothDeviceList* devices);

  // Requests bluetooth start discovering devices.
  virtual void BluetoothStartDiscovering();

  // Requests bluetooth stop discovering devices.
  virtual void BluetoothStopDiscovering();

  // Connect to a specific bluetooth device.
  virtual void ConnectToBluetoothDevice(const std::string& address);

  // Returns true if bluetooth adapter is discovering bluetooth devices.
  virtual bool IsBluetoothDiscovering();

  // Returns the currently selected IME.
  virtual void GetCurrentIME(IMEInfo* info);

  // Returns a list of availble IMEs.
  virtual void GetAvailableIMEList(IMEInfoList* list);

  // Returns a list of properties for the currently selected IME.
  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list);

  // Switches to the selected input method.
  virtual void SwitchIME(const std::string& ime_id);

  // Activates an IME property.
  virtual void ActivateIMEProperty(const std::string& key);

  // Shows UI to manage bluetooth devices.
  virtual void ManageBluetoothDevices();

  // Toggles bluetooth.
  virtual void ToggleBluetooth();

  // Shows UI to connect to an unlisted network of type |type|. On Chrome OS
  // |type| corresponds to a Shill network type.
  virtual void ShowOtherNetworkDialog(const std::string& type);

  // Returns whether bluetooth capability is available.
  virtual bool GetBluetoothAvailable();

  // Returns whether bluetooth is enabled.
  virtual bool GetBluetoothEnabled();

  // Returns whether the delegate has initiated a bluetooth discovery session.
  virtual bool GetBluetoothDiscovering();

  // Shows UI for changing proxy settings.
  virtual void ChangeProxySettings();

  // Returns NetworkingConfigDelegate. May return nullptr.
  virtual NetworkingConfigDelegate* GetNetworkingConfigDelegate() const;

  // Returns VolumeControlDelegate. May return nullptr.
  virtual VolumeControlDelegate* GetVolumeControlDelegate() const;

  // Sets the VolumeControlDelegate.
  virtual void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate);

  // Retrieves the session start time. Returns |false| if the time is not set.
  virtual bool GetSessionStartTime(base::TimeTicks* session_start_time);

  // Retrieves the session length limit. Returns |false| if no limit is set.
  virtual bool GetSessionLengthLimit(base::TimeDelta* session_length_limit);

  // Get the system tray menu size in pixels (dependent on the language).
  virtual int GetSystemTrayMenuWidth();

  // The active user has been changed. This will be called when the UI is ready
  // to be switched to the new user.
  // Note: This will happen after SessionStateObserver::ActiveUserChanged fires.
  virtual void ActiveUserWasChanged();

  // Returns true when the Search key is configured to be treated as Caps Lock.
  virtual bool IsSearchKeyMappedToCapsLock();

  // Returns accounts delegate for given user. May return nullptr.
  virtual tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id);

  // Adding observers that are notified when supervised info is being changed.
  virtual void AddCustodianInfoTrayObserver(
      CustodianInfoTrayObserver* observer);

  virtual void RemoveCustodianInfoTrayObserver(
      CustodianInfoTrayObserver* observer);

  // Adds an observer whose |OnShutdownPolicyChanged| function is called when
  // the |DeviceRebootOnShutdown| policy changes. If this policy is set to
  // true, a device cannot be shut down anymore but only rebooted.
  virtual void AddShutdownPolicyObserver(ShutdownPolicyObserver* observer);

  virtual void RemoveShutdownPolicyObserver(ShutdownPolicyObserver* observer);

  // Determines whether the device is automatically rebooted when shut down as
  // specified by the device policy |DeviceRebootOnShutdown|. This function
  // asynchronously calls |callback| once a trusted policy becomes available.
  virtual void ShouldRebootOnShutdown(const RebootOnShutdownCallback& callback);

  // Returns VPNDelegate. May return nullptr.
  virtual VPNDelegate* GetVPNDelegate() const;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_

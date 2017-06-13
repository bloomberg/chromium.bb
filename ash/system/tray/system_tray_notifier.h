// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

#include <string>

#include "ash/accessibility_types.h"
#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"

namespace ash {

class AccessibilityObserver;
class BluetoothObserver;
class ClockObserver;
class EnterpriseDomainObserver;
class IMEObserver;
class LastWindowClosedObserver;
class LogoutButtonObserver;
class NetworkObserver;
class NetworkPortalDetectorObserver;
class ScreenCaptureObserver;
class ScreenShareObserver;
class StatusAreaFocusObserver;
class TracingObserver;
class VirtualKeyboardObserver;
class UpdateObserver;

namespace mojom {
enum class UpdateSeverity;
}

// Observer and notification manager for the ash system tray.
class ASH_EXPORT SystemTrayNotifier {
 public:
  SystemTrayNotifier();
  ~SystemTrayNotifier();

  // Accessibility.
  void AddAccessibilityObserver(AccessibilityObserver* observer);
  void RemoveAccessibilityObserver(AccessibilityObserver* observer);
  void NotifyAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify);

  // Bluetooth.
  void AddBluetoothObserver(BluetoothObserver* observer);
  void RemoveBluetoothObserver(BluetoothObserver* observer);
  void NotifyRefreshBluetooth();
  void NotifyBluetoothDiscoveringChanged();

  // Date and time.
  void AddClockObserver(ClockObserver* observer);
  void RemoveClockObserver(ClockObserver* observer);
  void NotifyRefreshClock();
  void NotifyDateFormatChanged();
  void NotifySystemClockTimeUpdated();
  void NotifySystemClockCanSetTimeChanged(bool can_set_time);

  // Enterprise domain.
  void AddEnterpriseDomainObserver(EnterpriseDomainObserver* observer);
  void RemoveEnterpriseDomainObserver(EnterpriseDomainObserver* observer);
  void NotifyEnterpriseDomainChanged();

  // Input methods.
  void AddIMEObserver(IMEObserver* observer);
  void RemoveIMEObserver(IMEObserver* observer);
  void NotifyRefreshIME();
  void NotifyRefreshIMEMenu(bool is_active);

  // Last window closed.
  void AddLastWindowClosedObserver(LastWindowClosedObserver* observer);
  void RemoveLastWindowClosedObserver(LastWindowClosedObserver* observer);
  void NotifyLastWindowClosed();

  // Logout button.
  void AddLogoutButtonObserver(LogoutButtonObserver* observer);
  void RemoveLogoutButtonObserver(LogoutButtonObserver* observer);
  void NotifyShowLoginButtonChanged(bool show_login_button);
  void NotifyLogoutDialogDurationChanged(base::TimeDelta duration);

  // Network.
  void AddNetworkObserver(NetworkObserver* observer);
  void RemoveNetworkObserver(NetworkObserver* observer);
  void NotifyRequestToggleWifi();

  // Network portal detector.
  void AddNetworkPortalDetectorObserver(
      NetworkPortalDetectorObserver* observer);
  void RemoveNetworkPortalDetectorObserver(
      NetworkPortalDetectorObserver* observer);
  void NotifyOnCaptivePortalDetected(const std::string& guid);

  // Screen capture.
  void AddScreenCaptureObserver(ScreenCaptureObserver* observer);
  void RemoveScreenCaptureObserver(ScreenCaptureObserver* observer);
  void NotifyScreenCaptureStart(const base::Closure& stop_callback,
                                const base::string16& sharing_app_name);
  void NotifyScreenCaptureStop();

  // Screen share.
  void AddScreenShareObserver(ScreenShareObserver* observer);
  void RemoveScreenShareObserver(ScreenShareObserver* observer);
  void NotifyScreenShareStart(const base::Closure& stop_callback,
                              const base::string16& helper_name);
  void NotifyScreenShareStop();

  // Status area focus.
  void AddStatusAreaFocusObserver(StatusAreaFocusObserver* observer);
  void RemoveStatusAreaFocusObserver(StatusAreaFocusObserver* observer);
  void NotifyFocusOut(bool reverse);

  // Tracing.
  void AddTracingObserver(TracingObserver* observer);
  void RemoveTracingObserver(TracingObserver* observer);
  void NotifyTracingModeChanged(bool value);

  // Update.
  void AddUpdateObserver(UpdateObserver* observer);
  void RemoveUpdateObserver(UpdateObserver* observer);
  void NotifyUpdateOverCellularTargetSet(bool success);

  // Virtual keyboard.
  void AddVirtualKeyboardObserver(VirtualKeyboardObserver* observer);
  void RemoveVirtualKeyboardObserver(VirtualKeyboardObserver* observer);
  void NotifyVirtualKeyboardSuppressionChanged(bool suppressed);

 private:
  base::ObserverList<AccessibilityObserver> accessibility_observers_;
  base::ObserverList<BluetoothObserver> bluetooth_observers_;
  base::ObserverList<ClockObserver> clock_observers_;
  base::ObserverList<EnterpriseDomainObserver> enterprise_domain_observers_;
  base::ObserverList<IMEObserver> ime_observers_;
  base::ObserverList<LastWindowClosedObserver> last_window_closed_observers_;
  base::ObserverList<LogoutButtonObserver> logout_button_observers_;
  base::ObserverList<NetworkObserver> network_observers_;
  base::ObserverList<NetworkPortalDetectorObserver>
      network_portal_detector_observers_;
  base::ObserverList<ScreenCaptureObserver> screen_capture_observers_;
  base::ObserverList<ScreenShareObserver> screen_share_observers_;
  base::ObserverList<StatusAreaFocusObserver> status_area_focus_observers_;
  base::ObserverList<TracingObserver> tracing_observers_;
  base::ObserverList<UpdateObserver> update_observers_;
  base::ObserverList<VirtualKeyboardObserver> virtual_keyboard_observers_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayNotifier);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

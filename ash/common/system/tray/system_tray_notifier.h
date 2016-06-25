// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/common/accessibility_types.h"
#include "ash/common/system/locale/locale_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"

#if defined(OS_CHROMEOS)
#include "base/strings/string16.h"
#include "base/time/time.h"
#endif

namespace ash {

class AccessibilityObserver;
class AudioObserver;
class ClockObserver;
class IMEObserver;
struct UpdateInfo;
class UpdateObserver;
class UserObserver;

#if defined(OS_CHROMEOS)
class BluetoothObserver;
class EnterpriseDomainObserver;
class LastWindowClosedObserver;
class LogoutButtonObserver;
class MediaCaptureObserver;
class NetworkObserver;
class NetworkPortalDetectorObserver;
class ScreenCaptureObserver;
class ScreenShareObserver;
class SessionLengthLimitObserver;
class TracingObserver;
class VirtualKeyboardObserver;
#endif

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

  // Audio.
  void AddAudioObserver(AudioObserver* observer);
  void RemoveAudioObserver(AudioObserver* observer);
  void NotifyAudioOutputVolumeChanged(uint64_t node_id, double volume);
  void NotifyAudioOutputMuteChanged(bool mute_on, bool system_adjust);
  void NotifyAudioNodesChanged();
  void NotifyAudioActiveOutputNodeChanged();
  void NotifyAudioActiveInputNodeChanged();

  // Date and time.
  void AddClockObserver(ClockObserver* observer);
  void RemoveClockObserver(ClockObserver* observer);
  void NotifyRefreshClock();
  void NotifyDateFormatChanged();
  void NotifySystemClockTimeUpdated();
  void NotifySystemClockCanSetTimeChanged(bool can_set_time);

  // Input methods.
  void AddIMEObserver(IMEObserver* observer);
  void RemoveIMEObserver(IMEObserver* observer);
  void NotifyRefreshIME();
  void NotifyRefreshIMEMenu(bool is_active);

  // Locale.
  void AddLocaleObserver(LocaleObserver* observer);
  void RemoveLocaleObserver(LocaleObserver* observer);
  void NotifyLocaleChanged(LocaleObserver::Delegate* delegate,
                           const std::string& cur_locale,
                           const std::string& from_locale,
                           const std::string& to_locale);

  // OS updates.
  void AddUpdateObserver(UpdateObserver* observer);
  void RemoveUpdateObserver(UpdateObserver* observer);
  void NotifyUpdateRecommended(const UpdateInfo& info);

  // User.
  void AddUserObserver(UserObserver* observer);
  void RemoveUserObserver(UserObserver* observer);
  void NotifyUserUpdate();
  void NotifyUserAddedToSession();

#if defined(OS_CHROMEOS)
  // Bluetooth.
  void AddBluetoothObserver(BluetoothObserver* observer);
  void RemoveBluetoothObserver(BluetoothObserver* observer);
  void NotifyRefreshBluetooth();
  void NotifyBluetoothDiscoveringChanged();

  // Enterprise domain.
  void AddEnterpriseDomainObserver(EnterpriseDomainObserver* observer);
  void RemoveEnterpriseDomainObserver(EnterpriseDomainObserver* observer);
  void NotifyEnterpriseDomainChanged();

  // Last window closed.
  void AddLastWindowClosedObserver(LastWindowClosedObserver* observer);
  void RemoveLastWindowClosedObserver(LastWindowClosedObserver* observer);
  void NotifyLastWindowClosed();

  // Logout button.
  void AddLogoutButtonObserver(LogoutButtonObserver* observer);
  void RemoveLogoutButtonObserver(LogoutButtonObserver* observer);
  void NotifyShowLoginButtonChanged(bool show_login_button);
  void NotifyLogoutDialogDurationChanged(base::TimeDelta duration);

  // Media capture.
  void AddMediaCaptureObserver(MediaCaptureObserver* observer);
  void RemoveMediaCaptureObserver(MediaCaptureObserver* observer);
  void NotifyMediaCaptureChanged();

  // Network.
  void AddNetworkObserver(NetworkObserver* observer);
  void RemoveNetworkObserver(NetworkObserver* observer);
  void NotifyRequestToggleWifi();

  // Network portal detector.
  void AddNetworkPortalDetectorObserver(
      NetworkPortalDetectorObserver* observer);
  void RemoveNetworkPortalDetectorObserver(
      NetworkPortalDetectorObserver* observer);
  void NotifyOnCaptivePortalDetected(const std::string& service_path);

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

  // Session length limit.
  void AddSessionLengthLimitObserver(SessionLengthLimitObserver* observer);
  void RemoveSessionLengthLimitObserver(SessionLengthLimitObserver* observer);
  void NotifySessionStartTimeChanged();
  void NotifySessionLengthLimitChanged();

  // Tracing.
  void AddTracingObserver(TracingObserver* observer);
  void RemoveTracingObserver(TracingObserver* observer);
  void NotifyTracingModeChanged(bool value);

  // Virtual keyboard.
  void AddVirtualKeyboardObserver(VirtualKeyboardObserver* observer);
  void RemoveVirtualKeyboardObserver(VirtualKeyboardObserver* observer);
  void NotifyVirtualKeyboardSuppressionChanged(bool suppressed);
#endif

 private:
  base::ObserverList<AccessibilityObserver> accessibility_observers_;
  base::ObserverList<AudioObserver> audio_observers_;
  base::ObserverList<ClockObserver> clock_observers_;
  base::ObserverList<IMEObserver> ime_observers_;
  base::ObserverList<LocaleObserver> locale_observers_;
  base::ObserverList<UpdateObserver> update_observers_;
  base::ObserverList<UserObserver> user_observers_;

#if defined(OS_CHROMEOS)
  base::ObserverList<BluetoothObserver> bluetooth_observers_;
  base::ObserverList<EnterpriseDomainObserver> enterprise_domain_observers_;
  base::ObserverList<LastWindowClosedObserver> last_window_closed_observers_;
  base::ObserverList<LogoutButtonObserver> logout_button_observers_;
  base::ObserverList<MediaCaptureObserver> media_capture_observers_;
  base::ObserverList<NetworkObserver> network_observers_;
  base::ObserverList<NetworkPortalDetectorObserver>
      network_portal_detector_observers_;
  base::ObserverList<ScreenCaptureObserver> screen_capture_observers_;
  base::ObserverList<ScreenShareObserver> screen_share_observers_;
  base::ObserverList<SessionLengthLimitObserver>
      session_length_limit_observers_;
  base::ObserverList<TracingObserver> tracing_observers_;
  base::ObserverList<VirtualKeyboardObserver> virtual_keyboard_observers_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SystemTrayNotifier);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

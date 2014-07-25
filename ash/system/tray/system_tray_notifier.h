// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/audio/audio_observer.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/chromeos/tray_tracing.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/locale/locale_observer.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/user/update_observer.h"
#include "ash/system/user/user_observer.h"
#include "base/observer_list.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/enterprise/enterprise_domain_observer.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/network/network_portal_detector_observer.h"
#include "ash/system/chromeos/screen_security/screen_capture_observer.h"
#include "ash/system/chromeos/screen_security/screen_share_observer.h"
#include "ash/system/chromeos/session/last_window_closed_observer.h"
#include "ash/system/chromeos/session/logout_button_observer.h"
#include "ash/system/chromeos/session/session_length_limit_observer.h"
#include "ash/system/tray/media_security/media_capture_observer.h"
#include "base/time/time.h"
#endif

namespace ash {

#if defined(OS_CHROMEOS)
class NetworkStateNotifier;
#endif

class ASH_EXPORT SystemTrayNotifier {
 public:
  SystemTrayNotifier();
  ~SystemTrayNotifier();

  void AddAccessibilityObserver(AccessibilityObserver* observer);
  void RemoveAccessibilityObserver(AccessibilityObserver* observer);

  void AddAudioObserver(AudioObserver* observer);
  void RemoveAudioObserver(AudioObserver* observer);

  void AddBluetoothObserver(BluetoothObserver* observer);
  void RemoveBluetoothObserver(BluetoothObserver* observer);

  void AddClockObserver(ClockObserver* observer);
  void RemoveClockObserver(ClockObserver* observer);

  void AddIMEObserver(IMEObserver* observer);
  void RemoveIMEObserver(IMEObserver* observer);

  void AddLocaleObserver(LocaleObserver* observer);
  void RemoveLocaleObserver(LocaleObserver* observer);

  void AddTracingObserver(TracingObserver* observer);
  void RemoveTracingObserver(TracingObserver* observer);

  void AddUpdateObserver(UpdateObserver* observer);
  void RemoveUpdateObserver(UpdateObserver* observer);

  void AddUserObserver(UserObserver* observer);
  void RemoveUserObserver(UserObserver* observer);

#if defined(OS_CHROMEOS)
  void AddLogoutButtonObserver(LogoutButtonObserver* observer);
  void RemoveLogoutButtonObserver(LogoutButtonObserver* observer);

  void AddSessionLengthLimitObserver(SessionLengthLimitObserver* observer);
  void RemoveSessionLengthLimitObserver(SessionLengthLimitObserver* observer);

  void AddNetworkObserver(NetworkObserver* observer);
  void RemoveNetworkObserver(NetworkObserver* observer);

  void AddNetworkPortalDetectorObserver(
      NetworkPortalDetectorObserver* observer);
  void RemoveNetworkPortalDetectorObserver(
      NetworkPortalDetectorObserver* observer);

  void AddEnterpriseDomainObserver(EnterpriseDomainObserver* observer);
  void RemoveEnterpriseDomainObserver(EnterpriseDomainObserver* observer);

  void AddMediaCaptureObserver(MediaCaptureObserver* observer);
  void RemoveMediaCaptureObserver(MediaCaptureObserver* observer);

  void AddScreenCaptureObserver(ScreenCaptureObserver* observer);
  void RemoveScreenCaptureObserver(ScreenCaptureObserver* observer);

  void AddScreenShareObserver(ScreenShareObserver* observer);
  void RemoveScreenShareObserver(ScreenShareObserver* observer);

  void AddLastWindowClosedObserver(LastWindowClosedObserver* observer);
  void RemoveLastWindowClosedObserver(LastWindowClosedObserver* observer);
#endif

  void NotifyAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify);
  void NotifyAudioOutputVolumeChanged();
  void NotifyAudioOutputMuteChanged();
  void NotifyAudioNodesChanged();
  void NotifyAudioActiveOutputNodeChanged();
  void NotifyAudioActiveInputNodeChanged();
  void NotifyTracingModeChanged(bool value);
  void NotifyRefreshBluetooth();
  void NotifyBluetoothDiscoveringChanged();
  void NotifyRefreshClock();
  void NotifyDateFormatChanged();
  void NotifySystemClockTimeUpdated();
  void NotifySystemClockCanSetTimeChanged(bool can_set_time);
  void NotifyRefreshIME();
  void NotifyLocaleChanged(LocaleObserver::Delegate* delegate,
                           const std::string& cur_locale,
                           const std::string& from_locale,
                           const std::string& to_locale);
  void NotifyUpdateRecommended(UpdateObserver::UpdateSeverity severity);
  void NotifyUserUpdate();
  void NotifyUserAddedToSession();
#if defined(OS_CHROMEOS)
  void NotifyShowLoginButtonChanged(bool show_login_button);
  void NotifyLogoutDialogDurationChanged(base::TimeDelta duration);
  void NotifySessionStartTimeChanged();
  void NotifySessionLengthLimitChanged();
  void NotifyRequestToggleWifi();
  void NotifyOnCaptivePortalDetected(const std::string& service_path);
  void NotifyEnterpriseDomainChanged();
  void NotifyMediaCaptureChanged();
  void NotifyScreenCaptureStart(const base::Closure& stop_callback,
                                const base::string16& sharing_app_name);
  void NotifyScreenCaptureStop();
  void NotifyScreenShareStart(const base::Closure& stop_callback,
                              const base::string16& helper_name);
  void NotifyScreenShareStop();
  void NotifyLastWindowClosed();

  NetworkStateNotifier* network_state_notifier() {
    return network_state_notifier_.get();
  }
#endif

 private:
  ObserverList<AccessibilityObserver> accessibility_observers_;
  ObserverList<AudioObserver> audio_observers_;
  ObserverList<BluetoothObserver> bluetooth_observers_;
  ObserverList<ClockObserver> clock_observers_;
  ObserverList<IMEObserver> ime_observers_;
  ObserverList<LocaleObserver> locale_observers_;
  ObserverList<TracingObserver> tracing_observers_;
  ObserverList<UpdateObserver> update_observers_;
  ObserverList<UserObserver> user_observers_;
#if defined(OS_CHROMEOS)
  ObserverList<LogoutButtonObserver> logout_button_observers_;
  ObserverList<SessionLengthLimitObserver> session_length_limit_observers_;
  ObserverList<NetworkObserver> network_observers_;
  ObserverList<NetworkPortalDetectorObserver>
      network_portal_detector_observers_;
  ObserverList<EnterpriseDomainObserver> enterprise_domain_observers_;
  ObserverList<MediaCaptureObserver> media_capture_observers_;
  ObserverList<ScreenCaptureObserver> screen_capture_observers_;
  ObserverList<ScreenShareObserver> screen_share_observers_;
  ObserverList<LastWindowClosedObserver> last_window_closed_observers_;
  scoped_ptr<NetworkStateNotifier> network_state_notifier_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SystemTrayNotifier);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

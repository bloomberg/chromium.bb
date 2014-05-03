// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_notifier.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/network/network_state_notifier.h"
#endif

namespace ash {

SystemTrayNotifier::SystemTrayNotifier() {
#if defined(OS_CHROMEOS)
  network_state_notifier_.reset(new NetworkStateNotifier());
#endif
}

SystemTrayNotifier::~SystemTrayNotifier() {
}

void SystemTrayNotifier::AddAccessibilityObserver(
    AccessibilityObserver* observer) {
  accessibility_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveAccessibilityObserver(
    AccessibilityObserver* observer) {
  accessibility_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddAudioObserver(AudioObserver* observer) {
  audio_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveAudioObserver(AudioObserver* observer) {
  audio_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddBluetoothObserver(BluetoothObserver* observer) {
  bluetooth_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveBluetoothObserver(BluetoothObserver* observer) {
  bluetooth_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddClockObserver(ClockObserver* observer) {
  clock_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveClockObserver(ClockObserver* observer) {
  clock_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddDriveObserver(DriveObserver* observer) {
  drive_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveDriveObserver(DriveObserver* observer) {
  drive_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddIMEObserver(IMEObserver* observer) {
  ime_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveIMEObserver(IMEObserver* observer) {
  ime_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddLocaleObserver(LocaleObserver* observer) {
  locale_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveLocaleObserver(LocaleObserver* observer) {
  locale_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddTracingObserver(TracingObserver* observer) {
  tracing_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveTracingObserver(TracingObserver* observer) {
  tracing_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddUpdateObserver(UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveUpdateObserver(UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddUserObserver(UserObserver* observer) {
  user_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveUserObserver(UserObserver* observer) {
  user_observers_.RemoveObserver(observer);
}

#if defined(OS_CHROMEOS)

void SystemTrayNotifier::AddLogoutButtonObserver(
    LogoutButtonObserver* observer) {
  logout_button_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveLogoutButtonObserver(
    LogoutButtonObserver* observer) {
  logout_button_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddSessionLengthLimitObserver(
    SessionLengthLimitObserver* observer) {
  session_length_limit_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveSessionLengthLimitObserver(
    SessionLengthLimitObserver* observer) {
  session_length_limit_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddNetworkObserver(NetworkObserver* observer) {
  network_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveNetworkObserver(NetworkObserver* observer) {
  network_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddNetworkPortalDetectorObserver(
    NetworkPortalDetectorObserver* observer) {
  network_portal_detector_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveNetworkPortalDetectorObserver(
    NetworkPortalDetectorObserver* observer) {
  network_portal_detector_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddEnterpriseDomainObserver(
    EnterpriseDomainObserver* observer) {
  enterprise_domain_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveEnterpriseDomainObserver(
    EnterpriseDomainObserver* observer) {
  enterprise_domain_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddMediaCaptureObserver(
    MediaCaptureObserver* observer) {
  media_capture_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveMediaCaptureObserver(
    MediaCaptureObserver* observer) {
  media_capture_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddScreenCaptureObserver(
    ScreenCaptureObserver* observer) {
  screen_capture_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveScreenCaptureObserver(
    ScreenCaptureObserver* observer) {
  screen_capture_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddScreenShareObserver(
    ScreenShareObserver* observer) {
  screen_share_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveScreenShareObserver(
    ScreenShareObserver* observer) {
  screen_share_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::AddLastWindowClosedObserver(
    LastWindowClosedObserver* observer) {
  last_window_closed_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveLastWindowClosedObserver(
    LastWindowClosedObserver* observer) {
  last_window_closed_observers_.RemoveObserver(observer);
}
#endif

void SystemTrayNotifier::NotifyAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  FOR_EACH_OBSERVER(
      AccessibilityObserver,
      accessibility_observers_,
      OnAccessibilityModeChanged(notify));
}

void SystemTrayNotifier::NotifyAudioOutputVolumeChanged() {
  FOR_EACH_OBSERVER(
      AudioObserver,
      audio_observers_,
      OnOutputVolumeChanged());
}

void SystemTrayNotifier::NotifyAudioOutputMuteChanged() {
  FOR_EACH_OBSERVER(
      AudioObserver,
      audio_observers_,
      OnOutputMuteChanged());
}

void SystemTrayNotifier::NotifyAudioNodesChanged() {
  FOR_EACH_OBSERVER(
      AudioObserver,
      audio_observers_,
      OnAudioNodesChanged());
}

void SystemTrayNotifier::NotifyAudioActiveOutputNodeChanged() {
  FOR_EACH_OBSERVER(
      AudioObserver,
      audio_observers_,
      OnActiveOutputNodeChanged());
}

void SystemTrayNotifier::NotifyAudioActiveInputNodeChanged() {
  FOR_EACH_OBSERVER(
      AudioObserver,
      audio_observers_,
      OnActiveInputNodeChanged());
}

void SystemTrayNotifier::NotifyTracingModeChanged(bool value) {
  FOR_EACH_OBSERVER(
      TracingObserver,
      tracing_observers_,
      OnTracingModeChanged(value));
}

void SystemTrayNotifier::NotifyRefreshBluetooth() {
  FOR_EACH_OBSERVER(BluetoothObserver,
                    bluetooth_observers_,
                    OnBluetoothRefresh());
}

void SystemTrayNotifier::NotifyBluetoothDiscoveringChanged() {
  FOR_EACH_OBSERVER(BluetoothObserver,
                    bluetooth_observers_,
                    OnBluetoothDiscoveringChanged());
}

void SystemTrayNotifier::NotifyRefreshClock() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_, Refresh());
}

void SystemTrayNotifier::NotifyDateFormatChanged() {
  FOR_EACH_OBSERVER(ClockObserver,
                    clock_observers_,
                    OnDateFormatChanged());
}

void SystemTrayNotifier::NotifySystemClockTimeUpdated() {
  FOR_EACH_OBSERVER(ClockObserver,
                    clock_observers_,
                    OnSystemClockTimeUpdated());
}

void SystemTrayNotifier::NotifySystemClockCanSetTimeChanged(bool can_set_time) {
  FOR_EACH_OBSERVER(ClockObserver,
                    clock_observers_,
                    OnSystemClockCanSetTimeChanged(can_set_time));
}

void SystemTrayNotifier::NotifyDriveJobUpdated(
    const DriveOperationStatus& status) {
  FOR_EACH_OBSERVER(DriveObserver,
                    drive_observers_,
                    OnDriveJobUpdated(status));
}

void SystemTrayNotifier::NotifyRefreshIME() {
  FOR_EACH_OBSERVER(IMEObserver,
                    ime_observers_,
                    OnIMERefresh());
}

void SystemTrayNotifier::NotifyLocaleChanged(
    LocaleObserver::Delegate* delegate,
    const std::string& cur_locale,
    const std::string& from_locale,
    const std::string& to_locale) {
  FOR_EACH_OBSERVER(
      LocaleObserver,
      locale_observers_,
      OnLocaleChanged(delegate, cur_locale, from_locale, to_locale));
}

void SystemTrayNotifier::NotifyUpdateRecommended(
    UpdateObserver::UpdateSeverity severity) {
  FOR_EACH_OBSERVER(UpdateObserver,
                    update_observers_,
                    OnUpdateRecommended(severity));
}

void SystemTrayNotifier::NotifyUserUpdate() {
  FOR_EACH_OBSERVER(UserObserver,
                    user_observers_,
                    OnUserUpdate());
}

void SystemTrayNotifier::NotifyUserAddedToSession() {
  FOR_EACH_OBSERVER(UserObserver,
                    user_observers_,
                    OnUserAddedToSession());
}

#if defined(OS_CHROMEOS)

void SystemTrayNotifier::NotifyShowLoginButtonChanged(bool show_login_button) {
  FOR_EACH_OBSERVER(LogoutButtonObserver,
                    logout_button_observers_,
                    OnShowLogoutButtonInTrayChanged(show_login_button));
}

void SystemTrayNotifier::NotifyLogoutDialogDurationChanged(
    base::TimeDelta duration) {
  FOR_EACH_OBSERVER(LogoutButtonObserver,
                    logout_button_observers_,
                    OnLogoutDialogDurationChanged(duration));
}

void SystemTrayNotifier::NotifySessionStartTimeChanged() {
  FOR_EACH_OBSERVER(SessionLengthLimitObserver,
                    session_length_limit_observers_,
                    OnSessionStartTimeChanged());
}

void SystemTrayNotifier::NotifySessionLengthLimitChanged() {
  FOR_EACH_OBSERVER(SessionLengthLimitObserver,
                    session_length_limit_observers_,
                    OnSessionLengthLimitChanged());
}

void SystemTrayNotifier::NotifyRequestToggleWifi() {
  FOR_EACH_OBSERVER(NetworkObserver,
                    network_observers_,
                    RequestToggleWifi());
}

void SystemTrayNotifier::NotifyOnCaptivePortalDetected(
    const std::string& service_path) {
  FOR_EACH_OBSERVER(NetworkPortalDetectorObserver,
                    network_portal_detector_observers_,
                    OnCaptivePortalDetected(service_path));
}

void SystemTrayNotifier::NotifyEnterpriseDomainChanged() {
  FOR_EACH_OBSERVER(EnterpriseDomainObserver, enterprise_domain_observers_,
      OnEnterpriseDomainChanged());
}

void SystemTrayNotifier::NotifyMediaCaptureChanged() {
  FOR_EACH_OBSERVER(
      MediaCaptureObserver, media_capture_observers_, OnMediaCaptureChanged());
}

void SystemTrayNotifier::NotifyScreenCaptureStart(
    const base::Closure& stop_callback,
    const base::string16& sharing_app_name) {
  FOR_EACH_OBSERVER(ScreenCaptureObserver, screen_capture_observers_,
                    OnScreenCaptureStart(stop_callback, sharing_app_name));
}

void SystemTrayNotifier::NotifyScreenCaptureStop() {
  FOR_EACH_OBSERVER(ScreenCaptureObserver, screen_capture_observers_,
                    OnScreenCaptureStop());
}

void SystemTrayNotifier::NotifyScreenShareStart(
    const base::Closure& stop_callback,
    const base::string16& helper_name) {
  FOR_EACH_OBSERVER(ScreenShareObserver, screen_share_observers_,
                    OnScreenShareStart(stop_callback, helper_name));
}

void SystemTrayNotifier::NotifyScreenShareStop() {
  FOR_EACH_OBSERVER(ScreenShareObserver, screen_share_observers_,
                    OnScreenShareStop());
}

void SystemTrayNotifier::NotifyLastWindowClosed() {
  FOR_EACH_OBSERVER(LastWindowClosedObserver,
                    last_window_closed_observers_,
                    OnLastWindowClosed());
}

#endif  // OS_CHROMEOS

}  // namespace ash

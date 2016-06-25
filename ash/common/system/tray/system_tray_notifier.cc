// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray_notifier.h"

#include "ash/common/system/accessibility_observer.h"
#include "ash/common/system/audio/audio_observer.h"
#include "ash/common/system/date/clock_observer.h"
#include "ash/common/system/ime/ime_observer.h"
#include "ash/common/system/update/update_observer.h"
#include "ash/common/system/user/user_observer.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/bluetooth/bluetooth_observer.h"
#include "ash/common/system/chromeos/enterprise/enterprise_domain_observer.h"
#include "ash/common/system/chromeos/media_security/media_capture_observer.h"
#include "ash/common/system/chromeos/network/network_observer.h"
#include "ash/common/system/chromeos/network/network_portal_detector_observer.h"
#include "ash/common/system/chromeos/screen_security/screen_capture_observer.h"
#include "ash/common/system/chromeos/screen_security/screen_share_observer.h"
#include "ash/common/system/chromeos/session/last_window_closed_observer.h"
#include "ash/common/system/chromeos/session/logout_button_observer.h"
#include "ash/common/system/chromeos/session/session_length_limit_observer.h"
#include "ash/common/system/chromeos/tray_tracing.h"
#include "ash/common/system/chromeos/virtual_keyboard/virtual_keyboard_observer.h"
#endif

namespace ash {

SystemTrayNotifier::SystemTrayNotifier() {}

SystemTrayNotifier::~SystemTrayNotifier() {}

void SystemTrayNotifier::AddAccessibilityObserver(
    AccessibilityObserver* observer) {
  accessibility_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveAccessibilityObserver(
    AccessibilityObserver* observer) {
  accessibility_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  FOR_EACH_OBSERVER(AccessibilityObserver, accessibility_observers_,
                    OnAccessibilityModeChanged(notify));
}

void SystemTrayNotifier::AddAudioObserver(AudioObserver* observer) {
  audio_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveAudioObserver(AudioObserver* observer) {
  audio_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyAudioOutputVolumeChanged(uint64_t node_id,
                                                        double volume) {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_,
                    OnOutputNodeVolumeChanged(node_id, volume));
}

void SystemTrayNotifier::NotifyAudioOutputMuteChanged(bool mute_on,
                                                      bool system_adjust) {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_,
                    OnOutputMuteChanged(mute_on, system_adjust));
}

void SystemTrayNotifier::NotifyAudioNodesChanged() {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_, OnAudioNodesChanged());
}

void SystemTrayNotifier::NotifyAudioActiveOutputNodeChanged() {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_,
                    OnActiveOutputNodeChanged());
}

void SystemTrayNotifier::NotifyAudioActiveInputNodeChanged() {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_,
                    OnActiveInputNodeChanged());
}

void SystemTrayNotifier::AddClockObserver(ClockObserver* observer) {
  clock_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveClockObserver(ClockObserver* observer) {
  clock_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRefreshClock() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_, Refresh());
}

void SystemTrayNotifier::NotifyDateFormatChanged() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_, OnDateFormatChanged());
}

void SystemTrayNotifier::NotifySystemClockTimeUpdated() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_,
                    OnSystemClockTimeUpdated());
}

void SystemTrayNotifier::NotifySystemClockCanSetTimeChanged(bool can_set_time) {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_,
                    OnSystemClockCanSetTimeChanged(can_set_time));
}

void SystemTrayNotifier::AddIMEObserver(IMEObserver* observer) {
  ime_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveIMEObserver(IMEObserver* observer) {
  ime_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRefreshIME() {
  FOR_EACH_OBSERVER(IMEObserver, ime_observers_, OnIMERefresh());
}

void SystemTrayNotifier::NotifyRefreshIMEMenu(bool is_active) {
  FOR_EACH_OBSERVER(IMEObserver, ime_observers_,
                    OnIMEMenuActivationChanged(is_active));
}

void SystemTrayNotifier::AddLocaleObserver(LocaleObserver* observer) {
  locale_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveLocaleObserver(LocaleObserver* observer) {
  locale_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyLocaleChanged(LocaleObserver::Delegate* delegate,
                                             const std::string& cur_locale,
                                             const std::string& from_locale,
                                             const std::string& to_locale) {
  FOR_EACH_OBSERVER(
      LocaleObserver, locale_observers_,
      OnLocaleChanged(delegate, cur_locale, from_locale, to_locale));
}

void SystemTrayNotifier::AddUpdateObserver(UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveUpdateObserver(UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyUpdateRecommended(const UpdateInfo& info) {
  FOR_EACH_OBSERVER(UpdateObserver, update_observers_,
                    OnUpdateRecommended(info));
}

void SystemTrayNotifier::AddUserObserver(UserObserver* observer) {
  user_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveUserObserver(UserObserver* observer) {
  user_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyUserUpdate() {
  FOR_EACH_OBSERVER(UserObserver, user_observers_, OnUserUpdate());
}

void SystemTrayNotifier::NotifyUserAddedToSession() {
  FOR_EACH_OBSERVER(UserObserver, user_observers_, OnUserAddedToSession());
}

#if defined(OS_CHROMEOS)

void SystemTrayNotifier::AddBluetoothObserver(BluetoothObserver* observer) {
  bluetooth_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveBluetoothObserver(BluetoothObserver* observer) {
  bluetooth_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRefreshBluetooth() {
  FOR_EACH_OBSERVER(BluetoothObserver, bluetooth_observers_,
                    OnBluetoothRefresh());
}

void SystemTrayNotifier::NotifyBluetoothDiscoveringChanged() {
  FOR_EACH_OBSERVER(BluetoothObserver, bluetooth_observers_,
                    OnBluetoothDiscoveringChanged());
}

void SystemTrayNotifier::AddEnterpriseDomainObserver(
    EnterpriseDomainObserver* observer) {
  enterprise_domain_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveEnterpriseDomainObserver(
    EnterpriseDomainObserver* observer) {
  enterprise_domain_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyEnterpriseDomainChanged() {
  FOR_EACH_OBSERVER(EnterpriseDomainObserver, enterprise_domain_observers_,
                    OnEnterpriseDomainChanged());
}

void SystemTrayNotifier::AddLastWindowClosedObserver(
    LastWindowClosedObserver* observer) {
  last_window_closed_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveLastWindowClosedObserver(
    LastWindowClosedObserver* observer) {
  last_window_closed_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyLastWindowClosed() {
  FOR_EACH_OBSERVER(LastWindowClosedObserver, last_window_closed_observers_,
                    OnLastWindowClosed());
}

void SystemTrayNotifier::AddLogoutButtonObserver(
    LogoutButtonObserver* observer) {
  logout_button_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveLogoutButtonObserver(
    LogoutButtonObserver* observer) {
  logout_button_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyShowLoginButtonChanged(bool show_login_button) {
  FOR_EACH_OBSERVER(LogoutButtonObserver, logout_button_observers_,
                    OnShowLogoutButtonInTrayChanged(show_login_button));
}

void SystemTrayNotifier::NotifyLogoutDialogDurationChanged(
    base::TimeDelta duration) {
  FOR_EACH_OBSERVER(LogoutButtonObserver, logout_button_observers_,
                    OnLogoutDialogDurationChanged(duration));
}

void SystemTrayNotifier::AddMediaCaptureObserver(
    MediaCaptureObserver* observer) {
  media_capture_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveMediaCaptureObserver(
    MediaCaptureObserver* observer) {
  media_capture_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyMediaCaptureChanged() {
  FOR_EACH_OBSERVER(MediaCaptureObserver, media_capture_observers_,
                    OnMediaCaptureChanged());
}

void SystemTrayNotifier::AddNetworkObserver(NetworkObserver* observer) {
  network_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveNetworkObserver(NetworkObserver* observer) {
  network_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRequestToggleWifi() {
  FOR_EACH_OBSERVER(NetworkObserver, network_observers_, RequestToggleWifi());
}

void SystemTrayNotifier::AddNetworkPortalDetectorObserver(
    NetworkPortalDetectorObserver* observer) {
  network_portal_detector_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveNetworkPortalDetectorObserver(
    NetworkPortalDetectorObserver* observer) {
  network_portal_detector_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyOnCaptivePortalDetected(
    const std::string& service_path) {
  FOR_EACH_OBSERVER(NetworkPortalDetectorObserver,
                    network_portal_detector_observers_,
                    OnCaptivePortalDetected(service_path));
}

void SystemTrayNotifier::AddScreenCaptureObserver(
    ScreenCaptureObserver* observer) {
  screen_capture_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveScreenCaptureObserver(
    ScreenCaptureObserver* observer) {
  screen_capture_observers_.RemoveObserver(observer);
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

void SystemTrayNotifier::AddScreenShareObserver(ScreenShareObserver* observer) {
  screen_share_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveScreenShareObserver(
    ScreenShareObserver* observer) {
  screen_share_observers_.RemoveObserver(observer);
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

void SystemTrayNotifier::AddSessionLengthLimitObserver(
    SessionLengthLimitObserver* observer) {
  session_length_limit_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveSessionLengthLimitObserver(
    SessionLengthLimitObserver* observer) {
  session_length_limit_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifySessionStartTimeChanged() {
  FOR_EACH_OBSERVER(SessionLengthLimitObserver, session_length_limit_observers_,
                    OnSessionStartTimeChanged());
}

void SystemTrayNotifier::NotifySessionLengthLimitChanged() {
  FOR_EACH_OBSERVER(SessionLengthLimitObserver, session_length_limit_observers_,
                    OnSessionLengthLimitChanged());
}

void SystemTrayNotifier::AddTracingObserver(TracingObserver* observer) {
  tracing_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveTracingObserver(TracingObserver* observer) {
  tracing_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyTracingModeChanged(bool value) {
  FOR_EACH_OBSERVER(TracingObserver, tracing_observers_,
                    OnTracingModeChanged(value));
}

void SystemTrayNotifier::AddVirtualKeyboardObserver(
    VirtualKeyboardObserver* observer) {
  virtual_keyboard_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveVirtualKeyboardObserver(
    VirtualKeyboardObserver* observer) {
  virtual_keyboard_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyVirtualKeyboardSuppressionChanged(
    bool suppressed) {
  FOR_EACH_OBSERVER(VirtualKeyboardObserver, virtual_keyboard_observers_,
                    OnKeyboardSuppressionChanged(suppressed));
}

#endif  // defined(OS_CHROMEOS)

}  // namespace ash

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_notifier.h"

#include "ash/system/accessibility_observer.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/network/network_portal_detector_observer.h"
#include "ash/system/screen_security/screen_capture_observer.h"
#include "ash/system/screen_security/screen_share_observer.h"
#include "ash/system/session/last_window_closed_observer.h"
#include "ash/system/status_area_focus_observer.h"
#include "ash/system/tray_tracing.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_observer.h"

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
  for (auto& observer : accessibility_observers_)
    observer.OnAccessibilityModeChanged(notify);
}

void SystemTrayNotifier::AddBluetoothObserver(BluetoothObserver* observer) {
  bluetooth_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveBluetoothObserver(BluetoothObserver* observer) {
  bluetooth_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRefreshBluetooth() {
  for (auto& observer : bluetooth_observers_)
    observer.OnBluetoothRefresh();
}

void SystemTrayNotifier::NotifyBluetoothDiscoveringChanged() {
  for (auto& observer : bluetooth_observers_)
    observer.OnBluetoothDiscoveringChanged();
}

void SystemTrayNotifier::AddClockObserver(ClockObserver* observer) {
  clock_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveClockObserver(ClockObserver* observer) {
  clock_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRefreshClock() {
  for (auto& observer : clock_observers_)
    observer.Refresh();
}

void SystemTrayNotifier::NotifyDateFormatChanged() {
  for (auto& observer : clock_observers_)
    observer.OnDateFormatChanged();
}

void SystemTrayNotifier::NotifySystemClockTimeUpdated() {
  for (auto& observer : clock_observers_)
    observer.OnSystemClockTimeUpdated();
}

void SystemTrayNotifier::NotifySystemClockCanSetTimeChanged(bool can_set_time) {
  for (auto& observer : clock_observers_)
    observer.OnSystemClockCanSetTimeChanged(can_set_time);
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
  for (auto& observer : enterprise_domain_observers_)
    observer.OnEnterpriseDomainChanged();
}

void SystemTrayNotifier::AddIMEObserver(IMEObserver* observer) {
  ime_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveIMEObserver(IMEObserver* observer) {
  ime_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRefreshIME() {
  for (auto& observer : ime_observers_)
    observer.OnIMERefresh();
}

void SystemTrayNotifier::NotifyRefreshIMEMenu(bool is_active) {
  for (auto& observer : ime_observers_)
    observer.OnIMEMenuActivationChanged(is_active);
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
  for (auto& observer : last_window_closed_observers_)
    observer.OnLastWindowClosed();
}

void SystemTrayNotifier::AddNetworkObserver(NetworkObserver* observer) {
  network_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveNetworkObserver(NetworkObserver* observer) {
  network_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRequestToggleWifi() {
  for (auto& observer : network_observers_)
    observer.RequestToggleWifi();
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
    const std::string& guid) {
  for (auto& observer : network_portal_detector_observers_)
    observer.OnCaptivePortalDetected(guid);
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
  for (auto& observer : screen_capture_observers_)
    observer.OnScreenCaptureStart(stop_callback, sharing_app_name);
}

void SystemTrayNotifier::NotifyScreenCaptureStop() {
  for (auto& observer : screen_capture_observers_)
    observer.OnScreenCaptureStop();
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
  for (auto& observer : screen_share_observers_)
    observer.OnScreenShareStart(stop_callback, helper_name);
}

void SystemTrayNotifier::NotifyScreenShareStop() {
  for (auto& observer : screen_share_observers_)
    observer.OnScreenShareStop();
}

void SystemTrayNotifier::AddStatusAreaFocusObserver(
    StatusAreaFocusObserver* observer) {
  status_area_focus_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveStatusAreaFocusObserver(
    StatusAreaFocusObserver* observer) {
  status_area_focus_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyFocusOut(bool reverse) {
  for (auto& observer : status_area_focus_observers_)
    observer.OnFocusOut(reverse);
}

void SystemTrayNotifier::AddTracingObserver(TracingObserver* observer) {
  tracing_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveTracingObserver(TracingObserver* observer) {
  tracing_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyTracingModeChanged(bool value) {
  for (auto& observer : tracing_observers_)
    observer.OnTracingModeChanged(value);
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
  for (auto& observer : virtual_keyboard_observers_)
    observer.OnKeyboardSuppressionChanged(suppressed);
}

}  // namespace ash

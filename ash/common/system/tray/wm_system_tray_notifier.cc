// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/wm_system_tray_notifier.h"

#include "ash/common/system/accessibility_observer.h"
#include "ash/common/system/audio/audio_observer.h"
#include "ash/common/system/date/clock_observer.h"
#include "ash/common/system/ime/ime_observer.h"
#include "ash/common/system/update/update_observer.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/virtual_keyboard/virtual_keyboard_observer.h"
#endif

namespace ash {

WmSystemTrayNotifier::WmSystemTrayNotifier() {}

WmSystemTrayNotifier::~WmSystemTrayNotifier() {}

void WmSystemTrayNotifier::AddAccessibilityObserver(
    AccessibilityObserver* observer) {
  accessibility_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveAccessibilityObserver(
    AccessibilityObserver* observer) {
  accessibility_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  FOR_EACH_OBSERVER(AccessibilityObserver, accessibility_observers_,
                    OnAccessibilityModeChanged(notify));
}

void WmSystemTrayNotifier::AddAudioObserver(AudioObserver* observer) {
  audio_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveAudioObserver(AudioObserver* observer) {
  audio_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyAudioOutputVolumeChanged(uint64_t node_id,
                                                          double volume) {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_,
                    OnOutputNodeVolumeChanged(node_id, volume));
}

void WmSystemTrayNotifier::NotifyAudioOutputMuteChanged(bool mute_on,
                                                        bool system_adjust) {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_,
                    OnOutputMuteChanged(mute_on, system_adjust));
}

void WmSystemTrayNotifier::NotifyAudioNodesChanged() {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_, OnAudioNodesChanged());
}

void WmSystemTrayNotifier::NotifyAudioActiveOutputNodeChanged() {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_,
                    OnActiveOutputNodeChanged());
}

void WmSystemTrayNotifier::NotifyAudioActiveInputNodeChanged() {
  FOR_EACH_OBSERVER(AudioObserver, audio_observers_,
                    OnActiveInputNodeChanged());
}

void WmSystemTrayNotifier::AddClockObserver(ClockObserver* observer) {
  clock_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveClockObserver(ClockObserver* observer) {
  clock_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyRefreshClock() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_, Refresh());
}

void WmSystemTrayNotifier::NotifyDateFormatChanged() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_, OnDateFormatChanged());
}

void WmSystemTrayNotifier::NotifySystemClockTimeUpdated() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_,
                    OnSystemClockTimeUpdated());
}

void WmSystemTrayNotifier::NotifySystemClockCanSetTimeChanged(
    bool can_set_time) {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_,
                    OnSystemClockCanSetTimeChanged(can_set_time));
}

void WmSystemTrayNotifier::AddIMEObserver(IMEObserver* observer) {
  ime_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveIMEObserver(IMEObserver* observer) {
  ime_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyRefreshIME() {
  FOR_EACH_OBSERVER(IMEObserver, ime_observers_, OnIMERefresh());
}

void WmSystemTrayNotifier::NotifyRefreshIMEMenu(bool is_active) {
  FOR_EACH_OBSERVER(IMEObserver, ime_observers_,
                    OnIMEMenuActivationChanged(is_active));
}

void WmSystemTrayNotifier::AddLocaleObserver(LocaleObserver* observer) {
  locale_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveLocaleObserver(LocaleObserver* observer) {
  locale_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyLocaleChanged(
    LocaleObserver::Delegate* delegate,
    const std::string& cur_locale,
    const std::string& from_locale,
    const std::string& to_locale) {
  FOR_EACH_OBSERVER(
      LocaleObserver, locale_observers_,
      OnLocaleChanged(delegate, cur_locale, from_locale, to_locale));
}

void WmSystemTrayNotifier::AddUpdateObserver(UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveUpdateObserver(UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyUpdateRecommended(const UpdateInfo& info) {
  FOR_EACH_OBSERVER(UpdateObserver, update_observers_,
                    OnUpdateRecommended(info));
}

#if defined(OS_CHROMEOS)

void WmSystemTrayNotifier::AddVirtualKeyboardObserver(
    VirtualKeyboardObserver* observer) {
  virtual_keyboard_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveVirtualKeyboardObserver(
    VirtualKeyboardObserver* observer) {
  virtual_keyboard_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyVirtualKeyboardSuppressionChanged(
    bool suppressed) {
  FOR_EACH_OBSERVER(VirtualKeyboardObserver, virtual_keyboard_observers_,
                    OnKeyboardSuppressionChanged(suppressed));
}

#endif  // defined(OS_CHROMEOS)

}  // namespace ash

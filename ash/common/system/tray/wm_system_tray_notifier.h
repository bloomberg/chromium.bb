// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_

#include "ash/ash_export.h"
#include "ash/common/accessibility_types.h"
#include "ash/common/system/locale/locale_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AccessibilityObserver;
class AudioObserver;
class ClockObserver;
class IMEObserver;
struct UpdateInfo;
class UpdateObserver;

#if defined(OS_CHROMEOS)
class VirtualKeyboardObserver;
#endif

// Observer and notification manager for the ash system tray. This version
// contains the observers that have been ported to //ash/common. See also
// ash::SystemTrayNotifier.
// TODO(jamescook): After all the observers have been moved, rename this class
// to SystemTrayNotifier.
class ASH_EXPORT WmSystemTrayNotifier {
 public:
  WmSystemTrayNotifier();
  ~WmSystemTrayNotifier();

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

#if defined(OS_CHROMEOS)
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

#if defined(OS_CHROMEOS)
  base::ObserverList<VirtualKeyboardObserver> virtual_keyboard_observers_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WmSystemTrayNotifier);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PALETTE_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_PALETTE_DELEGATE_CHROMEOS_H_

#include <string>

#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_state_observer.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/events/devices/input_device_event_observer.h"

class PrefChangeRegistrar;
class Profile;

namespace ash {
class ScopedSessionStateObserver;
}

namespace chromeos {

// A class which allows the Ash palette to perform chrome actions.
class PaletteDelegateChromeOS : public ash::PaletteDelegate,
                                public ui::InputDeviceEventObserver,
                                public ash::SessionStateObserver,
                                public content::NotificationObserver {
 public:
  // Attempts to create a palette delegate. This will return null if the palette
  // feature is not enabled.
  static std::unique_ptr<PaletteDelegateChromeOS> Create();

  ~PaletteDelegateChromeOS() override;

 private:
  PaletteDelegateChromeOS();

  // ash::PaletteDelegate:
  std::unique_ptr<EnableListenerSubscription> AddPaletteEnableListener(
      const EnableListener& on_state_changed) override;
  void CreateNote() override;
  bool HasNoteApp() override;
  void SetPartialMagnifierState(bool enabled) override;
  void SetStylusStateChangedCallback(
      const OnStylusStateChangedCallback& on_stylus_state_changed) override;
  bool ShouldAutoOpenPalette() override;
  bool ShouldShowPalette() override;
  void TakeScreenshot() override;
  void TakePartialScreenshot(const base::Closure& done) override;
  void CancelPartialScreenshot() override;

  // ash::SessionStateObserver:
  void ActiveUserChanged(const AccountId& account_id) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ui::InputDeviceObserver:
  void OnStylusStateChanged(ui::StylusState state) override;

  // Called when the palette enabled pref has changed.
  void OnPaletteEnabledPrefChanged();

  void SetProfile(Profile* profile);
  void OnPartialScreenshotDone(const base::Closure& then);

  base::CallbackList<void(bool)> palette_enabled_callback_list_;
  OnStylusStateChangedCallback on_stylus_state_changed_;

  // Unowned pointer to the active profile.
  Profile* profile_ = nullptr;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<ash::ScopedSessionStateObserver> session_state_observer_;
  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<PaletteDelegateChromeOS> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PaletteDelegateChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_PALETTE_DELEGATE_CHROMEOS_H_

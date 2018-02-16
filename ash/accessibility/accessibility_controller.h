// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_

#include <memory>

#include "ash/ash_constants.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_types.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/accessibility/ax_enums.mojom.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace service_manager {
class Connector;
}

namespace ash {

class ScopedBacklightsForcedOff;

// The controller for accessibility features in ash. Features can be enabled
// in chrome's webui settings or the system tray menu (see TrayAccessibility).
// Uses preferences to communicate with chrome to support mash.
class ASH_EXPORT AccessibilityController
    : public mojom::AccessibilityController,
      public SessionObserver {
 public:
  explicit AccessibilityController(service_manager::Connector* connector);
  ~AccessibilityController() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // Binds the mojom::AccessibilityController interface to this object.
  void BindRequest(mojom::AccessibilityControllerRequest request);

  void SetAutoclickEnabled(bool enabled);
  bool IsAutoclickEnabled() const;

  void SetHighContrastEnabled(bool enabled);
  bool IsHighContrastEnabled() const;

  void SetLargeCursorEnabled(bool enabled);
  bool IsLargeCursorEnabled() const;

  void SetMonoAudioEnabled(bool enabled);
  bool IsMonoAudioEnabled() const;

  void SetSpokenFeedbackEnabled(bool enabled,
                                AccessibilityNotificationVisibility notify);
  bool IsSpokenFeedbackEnabled() const;

  void SetSelectToSpeakEnabled(bool enabled);
  bool IsSelectToSpeakEnabled() const;

  void SetStickyKeysEnabled(bool enabled);
  bool IsStickyKeysEnabled() const;

  void SetVirtualKeyboardEnabled(bool enabled);
  bool IsVirtualKeyboardEnabled() const;

  bool braille_display_connected() const { return braille_display_connected_; }

  // Triggers an accessibility alert to give the user feedback.
  void TriggerAccessibilityAlert(mojom::AccessibilityAlert alert);

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // that their mapped event has occurred. The |sound_key| enums can be found in
  // chromeos/audio/chromeos_sounds.h.
  void PlayEarcon(int32_t sound_key);

  // Initiates play of shutdown sound. The base::TimeDelta parameter gets the
  // shutdown duration.
  void PlayShutdownSound(base::OnceCallback<void(base::TimeDelta)> callback);

  // Forwards an accessibility gesture from the touch exploration controller to
  // ChromeVox.
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture);

  // Toggle dictation.
  void ToggleDictation();

  // Whether or not to enable toggling spoken feedback via holding down two
  // fingers on the screen.
  void ShouldToggleSpokenFeedbackViaTouch(
      base::OnceCallback<void(bool)> callback);

  // Plays tick sound indicating spoken feedback will be toggled after
  // countdown.
  void PlaySpokenFeedbackToggleCountdown(int tick_count);

  // mojom::AccessibilityController:
  void SetClient(mojom::AccessibilityControllerClientPtr client) override;
  void SetDarkenScreen(bool darken) override;
  void BrailleDisplayStateChanged(bool connected) override;

  // SessionObserver:
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Test helpers:
  void FlushMojoForTest();

 private:
  // Observes either the signin screen prefs or active user prefs and loads
  // initial settings.
  void ObservePrefs(PrefService* prefs);

  void UpdateAutoclickFromPref();
  void UpdateAutoclickDelayFromPref();
  void UpdateHighContrastFromPref();
  void UpdateLargeCursorFromPref();
  void UpdateMonoAudioFromPref();
  void UpdateSpokenFeedbackFromPref();
  void UpdateSelectToSpeakFromPref();
  void UpdateStickyKeysFromPref();
  void UpdateVirtualKeyboardFromPref();

  service_manager::Connector* connector_ = nullptr;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Binding for mojom::AccessibilityController interface.
  mojo::BindingSet<mojom::AccessibilityController> bindings_;

  // Client interface in chrome browser.
  mojom::AccessibilityControllerClientPtr client_;

  bool autoclick_enabled_ = false;
  base::TimeDelta autoclick_delay_;
  bool high_contrast_enabled_ = false;
  bool large_cursor_enabled_ = false;
  int large_cursor_size_in_dip_ = kDefaultLargeCursorSize;
  bool mono_audio_enabled_ = false;
  bool spoken_feedback_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool sticky_keys_enabled_ = false;
  bool virtual_keyboard_enabled_ = false;
  bool braille_display_connected_ = false;

  // TODO(warx): consider removing this and replacing it with a more reliable
  // way (https://crbug.com/800270).
  AccessibilityNotificationVisibility spoken_feedback_notification_ =
      A11Y_NOTIFICATION_NONE;

  // Used to force the backlights off to darken the screen.
  std::unique_ptr<ScopedBacklightsForcedOff> scoped_backlights_forced_off_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityController);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_

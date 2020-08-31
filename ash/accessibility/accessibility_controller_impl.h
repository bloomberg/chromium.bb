// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace ax {
namespace mojom {
enum class Gesture;
}  // namespace mojom
}  // namespace ax

namespace gfx {
class Point;
struct VectorIcon;
}  // namespace gfx

namespace ash {

class AccessibilityHighlightController;
class AccessibilityObserver;
class FloatingAccessibilityController;
class ScopedBacklightsForcedOff;
class SelectToSpeakEventHandler;
class SwitchAccessMenuBubbleController;
class SwitchAccessEventHandler;

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

// The controller for accessibility features in ash. Features can be enabled
// in chrome's webui settings or the system tray menu (see TrayAccessibility).
// Uses preferences to communicate with chrome to support mash.
class ASH_EXPORT AccessibilityControllerImpl : public AccessibilityController,
                                               public SessionObserver,
                                               public TabletModeObserver {
 public:
  enum FeatureType {
    kAutoclick = 0,
    kCaretHighlight,
    KCursorHighlight,
    kDictation,
    kFloatingMenu,
    kFocusHighlight,
    kFullscreenMagnifier,
    kDockedMagnifier,
    kHighContrast,
    kLargeCursor,
    kMonoAudio,
    kSpokenFeedback,
    kSelectToSpeak,
    kStickyKeys,
    kSwitchAccess,
    kVirtualKeyboard,

    kFeatureCount,
    kNoConflictingFeature
  };

  // Common interface for all features.
  class Feature {
   public:
    Feature(FeatureType type,
            const std::string& pref_name,
            const gfx::VectorIcon* icon,
            AccessibilityControllerImpl* controller);
    Feature(const Feature&) = delete;
    Feature& operator=(Feature const&) = delete;
    virtual ~Feature();

    FeatureType type() const { return type_; }
    // Tries to set the feature to |enabled| by setting the user pref.
    // Setting feature to be enabled can fail in following conditions:
    // - there is a higher priority pref(managed), which overrides this value.
    // - there is an other feature, which conflicts with the current one.
    virtual void SetEnabled(bool enabled);
    bool enabled() const { return enabled_; }
    bool IsVisibleInTray() const;
    bool IsEnterpriseIconVisible() const;
    const std::string& pref_name() const { return pref_name_; }
    const gfx::VectorIcon& icon() const;

    void UpdateFromPref();
    void SetConflictingFeature(FeatureType feature);

   protected:
    const FeatureType type_;
    // Some features cannot be enabled while others are on. When a conflicting
    // feature is enabled, we cannot enable current feature.
    FeatureType conflicting_feature_ = FeatureType::kNoConflictingFeature;
    bool enabled_ = false;
    const std::string pref_name_;
    const gfx::VectorIcon* icon_;
    AccessibilityControllerImpl* const owner_;
  };

  // Helper struct to store information about a11y dialog -- pref name, resource
  // ids for title and body. Also stores the information whether this dialog is
  // mandatory for every SetEnabled call.
  struct Dialog {
    std::string pref_name;
    int title_resource_id;
    int body_resource_id;
    // Whether this dialog should be shown on every SetEnabled action.
    bool mandatory;
  };

  // Some features have confirmation dialog associated with them.
  // Dialog can be applied for all SetEnabled() actions, or only to ones
  // associated with accelerators.
  class FeatureWithDialog : public Feature {
   public:
    FeatureWithDialog(FeatureType type,
                      const std::string& pref_name,
                      const gfx::VectorIcon* icon,
                      const Dialog& dialog,
                      AccessibilityControllerImpl* controller);
    ~FeatureWithDialog() override;

    // Tries to set the feature enabled, if its dialog is mandatory, shows the
    // dailog for the first time feature is enabled.
    void SetEnabled(bool enabled) override;
    // If the dialog have not been accepted, we show it. When it is accepted, we
    // call SetEnabled() and invoke |completion_callback|.
    void SetEnabledWithDialog(bool enabled,
                              base::OnceClosure completion_callback);
    void SetDialogAccepted();
    bool WasDialogAccepted() const;

   private:
    Dialog dialog_;
  };

  AccessibilityControllerImpl();
  ~AccessibilityControllerImpl() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void Shutdown();

  void AddObserver(AccessibilityObserver* observer);
  void RemoveObserver(AccessibilityObserver* observer);

  Feature& GetFeature(FeatureType feature) const;

  // Getters for the corresponding features.
  Feature& autoclick() const;
  Feature& caret_highlight() const;
  Feature& cursor_highlight() const;
  FeatureWithDialog& dictation() const;
  Feature& floating_menu() const;
  Feature& focus_highlight() const;
  FeatureWithDialog& fullscreen_magnifier() const;
  FeatureWithDialog& docked_magnifier() const;
  FeatureWithDialog& high_contrast() const;
  Feature& large_cursor() const;
  Feature& mono_audio() const;
  Feature& spoken_feedback() const;
  Feature& select_to_speak() const;
  Feature& sticky_keys() const;
  Feature& switch_access() const;
  Feature& virtual_keyboard() const;

  // The following functions read and write to their associated preference.
  // These values are then used to determine whether the accelerator
  // confirmation dialog for the respective preference has been accepted before.
  void SetHighContrastAcceleratorDialogAccepted();
  bool HasHighContrastAcceleratorDialogBeenAccepted() const;
  void SetScreenMagnifierAcceleratorDialogAccepted();
  bool HasScreenMagnifierAcceleratorDialogBeenAccepted() const;
  void SetDockedMagnifierAcceleratorDialogAccepted();
  bool HasDockedMagnifierAcceleratorDialogBeenAccepted() const;
  void SetDictationAcceleratorDialogAccepted();
  bool HasDictationAcceleratorDialogBeenAccepted() const;
  void SetDisplayRotationAcceleratorDialogBeenAccepted();
  bool HasDisplayRotationAcceleratorDialogBeenAccepted() const;

  void SetAutoclickEnabled(bool enabled);
  bool autoclick_enabled() const { return autoclick().enabled(); }
  bool IsAutoclickSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForAutoclick();

  void SetAutoclickEventType(AutoclickEventType event_type);
  AutoclickEventType GetAutoclickEventType();
  void SetAutoclickMenuPosition(FloatingMenuPosition position);
  FloatingMenuPosition GetAutoclickMenuPosition();
  void RequestAutoclickScrollableBoundsForPoint(gfx::Point& point_in_screen);

  void SetFloatingMenuPosition(FloatingMenuPosition position);
  FloatingMenuPosition GetFloatingMenuPosition();
  FloatingAccessibilityController* GetFloatingMenuController();

  // Update the autoclick menu bounds if necessary. This may need to happen when
  // the display work area changes, or if system ui regions change (like the
  // virtual keyboard position).
  void UpdateAutoclickMenuBoundsIfNeeded();

  void SetCaretHighlightEnabled(bool enabled);
  bool caret_highlight_enabled() const { return caret_highlight().enabled(); }
  bool IsCaretHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForCaretHighlight();

  void SetCursorHighlightEnabled(bool enabled);
  bool cursor_highlight_enabled() const { return cursor_highlight().enabled(); }
  bool IsCursorHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForCursorHighlight();

  void SetDictationEnabled(bool enabled);
  bool dictation_enabled() const { return dictation().enabled(); }
  bool IsDictationSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForDictation();

  void SetFocusHighlightEnabled(bool enabled);
  bool focus_highlight_enabled() const { return focus_highlight().enabled(); }
  bool IsFocusHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFocusHighlight();

  void SetFullscreenMagnifierEnabled(bool enabled);
  bool IsFullscreenMagnifierEnabledForTesting();
  bool IsFullScreenMagnifierSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFullScreenMagnifier();

  void SetDockedMagnifierEnabledForTesting(bool enabled);
  bool IsDockedMagnifierEnabledForTesting();
  bool IsDockedMagnifierSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForDockedMagnifier();

  void SetHighContrastEnabled(bool enabled);
  bool high_contrast_enabled() const { return high_contrast().enabled(); }
  bool IsHighContrastSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForHighContrast();

  void SetLargeCursorEnabled(bool enabled);
  bool large_cursor_enabled() const { return large_cursor().enabled(); }
  bool IsLargeCursorSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForLargeCursor();

  void SetMonoAudioEnabled(bool enabled);
  bool mono_audio_enabled() const { return mono_audio().enabled(); }
  bool IsMonoAudioSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForMonoAudio();

  void SetSpokenFeedbackEnabled(bool enabled,
                                AccessibilityNotificationVisibility notify);
  bool spoken_feedback_enabled() const { return spoken_feedback().enabled(); }
  bool IsSpokenFeedbackSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSpokenFeedback();

  void SetSelectToSpeakEnabled(bool enabled);
  bool select_to_speak_enabled() const { return select_to_speak().enabled(); }
  bool IsSelectToSpeakSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSelectToSpeak();

  void RequestSelectToSpeakStateChange();
  SelectToSpeakState GetSelectToSpeakState() const;

  void SetStickyKeysEnabled(bool enabled);
  bool sticky_keys_enabled() const { return sticky_keys().enabled(); }
  bool IsStickyKeysSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForStickyKeys();

  void SetSwitchAccessEnabled(bool enabled);
  bool switch_access_enabled() const { return switch_access().enabled(); }
  bool IsSwitchAccessSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSwitchAccess();

  void SetVirtualKeyboardEnabled(bool enabled);
  bool virtual_keyboard_enabled() const { return virtual_keyboard().enabled(); }
  bool IsVirtualKeyboardSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForVirtualKeyboard();

  void SetTabletModeShelfNavigationButtonsEnabled(bool enabled);
  bool tablet_mode_shelf_navigation_buttons_enabled() const {
    return tablet_mode_shelf_navigation_buttons_enabled_;
  }

  void ShowFloatingMenuIfEnabled() override;

  bool dictation_active() const { return dictation_active_; }

  // Returns true if accessibility shortcuts have been disabled.
  bool accessibility_shortcuts_enabled() const { return shortcuts_enabled_; }

  // Triggers an accessibility alert to give the user feedback.
  void TriggerAccessibilityAlert(AccessibilityAlert alert);

  // Triggers an accessibility alert with the given |message|.
  void TriggerAccessibilityAlertWithMessage(const std::string& message);

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // that their mapped event has occurred. The |sound_key| enums can be found in
  // chromeos/audio/chromeos_sounds.h.
  void PlayEarcon(int sound_key);

  // Initiates play of shutdown sound. Returns the TimeDelta duration.
  base::TimeDelta PlayShutdownSound();

  // Forwards an accessibility gesture from the touch exploration controller to
  // ChromeVox.
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture);

  // Toggle dictation.
  void ToggleDictation();

  // Cancels all current and queued speech immediately.
  void SilenceSpokenFeedback();

  // Called when we first detect two fingers are held down, which can be used to
  // toggle spoken feedback on some touch-only devices.
  void OnTwoFingerTouchStart();

  // Called when the user is no longer holding down two fingers (including
  // releasing one, holding down three, or moving them).
  void OnTwoFingerTouchStop();

  // Whether or not to enable toggling spoken feedback via holding down two
  // fingers on the screen.
  bool ShouldToggleSpokenFeedbackViaTouch() const;

  // Plays tick sound indicating spoken feedback will be toggled after
  // countdown.
  void PlaySpokenFeedbackToggleCountdown(int tick_count);

  // Returns true if that accessibility feature pref |path| is being controlled
  // by a policy and false otherwise.
  bool IsEnterpriseIconVisibleInTrayMenu(const std::string& path);

  // Returns true if at least one of the primary settings of the accessibility
  // features is going to be visible in the accessibility tray menu.
  bool IsPrimarySettingsViewVisibleInTray();

  // Returns true if at least one of the additional settings of the
  // accessibility features is going to be visible in the accessibility tray
  // menu.
  bool IsAdditionalSettingsViewVisibleInTray();

  // Returns true if there exist one of the additional accessibility features
  // and one of the primary accessibility features which are going to visible on
  // accessibility tray menu.
  bool IsAdditionalSettingsSeparatorVisibleInTray();

  // AccessibilityController:
  void SetClient(AccessibilityControllerClient* client) override;
  void SetDarkenScreen(bool darken) override;
  void BrailleDisplayStateChanged(bool connected) override;
  void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen) override;
  void SetCaretBounds(const gfx::Rect& bounds_in_screen) override;
  void SetAccessibilityPanelAlwaysVisible(bool always_visible) override;
  void SetAccessibilityPanelBounds(const gfx::Rect& bounds,
                                   AccessibilityPanelState state) override;
  void SetSelectToSpeakState(SelectToSpeakState state) override;
  void SetSelectToSpeakEventHandlerDelegate(
      SelectToSpeakEventHandlerDelegate* delegate) override;
  void SetSwitchAccessEventHandlerDelegate(
      SwitchAccessEventHandlerDelegate* delegate) override;
  void HideSwitchAccessBackButton() override;
  void HideSwitchAccessMenu() override;
  void ShowSwitchAccessBackButton(const gfx::Rect& anchor) override;
  void ShowSwitchAccessMenu(const gfx::Rect& anchor,
                            std::vector<std::string> actions_to_show) override;
  void SetDictationActive(bool is_active) override;
  void ToggleDictationFromSource(DictationToggleSource source) override;
  void OnAutoclickScrollableBoundsFound(gfx::Rect& bounds_in_screen) override;
  void ForwardKeyEventsToSwitchAccess(bool should_forward) override;
  base::string16 GetBatteryDescription() const override;
  void SetVirtualKeyboardVisible(bool is_visible) override;
  void NotifyAccessibilityStatusChanged() override;
  bool IsAccessibilityFeatureVisibleInTrayMenu(
      const std::string& path) override;
  void SetSwitchAccessIgnoreVirtualKeyEventForTesting(
      bool should_ignore) override;
  void DisablePolicyRecommendationRestorerForTesting() override;

  // SessionObserver:
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Test helpers:
  SwitchAccessEventHandler* GetSwitchAccessEventHandlerForTest();
  SwitchAccessMenuBubbleController* GetSwitchAccessBubbleControllerForTest() {
    return switch_access_bubble_controller_.get();
  }
  void no_switch_access_disable_confirmation_dialog_for_testing(
      bool skip_dialog) {
    no_switch_access_disable_confirmation_dialog_for_testing_ = skip_dialog;
  }

 private:
  // Populate |features_| with the feature of the correct type.
  void CreateAccessibilityFeatures();

  // Propagates the state of |feature| according to |feature->enabled()|.
  void OnFeatureChanged(FeatureType feature);

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Observes either the signin screen prefs or active user prefs and loads
  // initial settings.
  void ObservePrefs(PrefService* prefs);

  // Updates the actual feature status based on the prefs value.
  void UpdateFeatureFromPref(FeatureType feature);

  void UpdateAutoclickDelayFromPref();
  void UpdateAutoclickEventTypeFromPref();
  void UpdateAutoclickRevertToLeftClickFromPref();
  void UpdateAutoclickStabilizePositionFromPref();
  void UpdateAutoclickMovementThresholdFromPref();
  void UpdateAutoclickMenuPositionFromPref();
  void UpdateFloatingMenuPositionFromPref();
  void UpdateLargeCursorFromPref();
  void UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand command);
  void UpdateSwitchAccessAutoScanEnabledFromPref();
  void UpdateSwitchAccessAutoScanSpeedFromPref();
  void UpdateSwitchAccessAutoScanKeyboardSpeedFromPref();
  void UpdateAccessibilityHighlightingFromPrefs();
  void UpdateShortcutsEnabledFromPref();
  void UpdateTabletModeShelfNavigationButtonsFromPref();

  void SwitchAccessDisableDialogClosed(bool disable_dialog_accepted);
  void MaybeCreateSelectToSpeakEventHandler();
  void MaybeCreateSwitchAccessEventHandler();

  // Client interface in chrome browser.
  AccessibilityControllerClient* client_ = nullptr;

  std::unique_ptr<Feature> features_[kFeatureCount];

  base::TimeDelta autoclick_delay_;
  int large_cursor_size_in_dip_ = kDefaultLargeCursorSize;

  bool dictation_active_ = false;
  bool shortcuts_enabled_ = true;
  bool tablet_mode_shelf_navigation_buttons_enabled_ = false;

  SelectToSpeakState select_to_speak_state_ =
      SelectToSpeakState::kSelectToSpeakStateInactive;
  std::unique_ptr<SelectToSpeakEventHandler> select_to_speak_event_handler_;
  SelectToSpeakEventHandlerDelegate* select_to_speak_event_handler_delegate_ =
      nullptr;

  // List of key codes that Switch Access should capture.
  std::vector<int> switch_access_keys_to_capture_;
  std::unique_ptr<SwitchAccessMenuBubbleController>
      switch_access_bubble_controller_;
  std::unique_ptr<SwitchAccessEventHandler> switch_access_event_handler_;
  SwitchAccessEventHandlerDelegate* switch_access_event_handler_delegate_ =
      nullptr;
  bool no_switch_access_disable_confirmation_dialog_for_testing_ = false;

  // Used to control the highlights of caret, cursor and focus.
  std::unique_ptr<AccessibilityHighlightController>
      accessibility_highlight_controller_;

  // Used to display accessibility floating menu.
  std::unique_ptr<FloatingAccessibilityController> floating_menu_controller_;
  // By default, floating accessibility menu is not shown unless
  // ShowFloatingMenuIfEnabled() is called. This is used in kiosk mode to
  // postpone the showing of the menu till the splash screen closes. This value
  // makes floating menu visible as soon as it is enabled.
  bool always_show_floating_menu_when_enabled_ = false;

  // Used to force the backlights off to darken the screen.
  std::unique_ptr<ScopedBacklightsForcedOff> scoped_backlights_forced_off_;

  base::ObserverList<AccessibilityObserver> observers_;

  // The pref service of the currently active user or the signin profile before
  // user logs in. Can be null in ash_unittests.
  PrefService* active_user_prefs_ = nullptr;
  // This has to be the first one to be destroyed so we don't get updates about
  // any prefs during destruction.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::WeakPtrFactory<AccessibilityControllerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerImpl);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_

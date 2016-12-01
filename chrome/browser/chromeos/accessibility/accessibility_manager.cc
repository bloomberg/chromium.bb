// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "ash/aura/wm_window_aura.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/autoclick/mus/public/interfaces/autoclick.mojom.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_shell.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_extension_loader.h"
#include "chrome/browser/chromeos/accessibility/accessibility_highlight_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/ui/accessibility_focus_ring_controller.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/audio/audio_a11y_controller.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/login/login_state.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/host_id.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "media/audio/sounds/sounds_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

using content::BrowserThread;
using content::RenderViewHost;
using extensions::api::braille_display_private::BrailleController;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::StubBrailleController;

namespace chromeos {

namespace {

// When this flag is set, system sounds will not be played.
const char kAshDisableSystemSounds[] = "ash-disable-system-sounds";

// When this flag is set, system sounds will be played whether the
// ChromeVox is enabled or not.
const char kAshEnableSystemSounds[] = "ash-enable-system-sounds";

static chromeos::AccessibilityManager* g_accessibility_manager = NULL;

static BrailleController* g_braille_controller_for_test = NULL;

BrailleController* GetBrailleController() {
  if (g_braille_controller_for_test)
    return g_braille_controller_for_test;
  // Don't use the real braille controller for tests to avoid automatically
  // starting ChromeVox which confuses some tests.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestType))
    return StubBrailleController::GetInstance();
  return BrailleController::GetInstance();
}

}  // namespace

class ChromeVoxPanelWidgetObserver : public views::WidgetObserver {
 public:
  ChromeVoxPanelWidgetObserver(views::Widget* widget,
                               AccessibilityManager* manager)
      : widget_(widget), manager_(manager) {
    widget_->AddObserver(this);
  }

  void OnWidgetClosing(views::Widget* widget) override {
    CHECK_EQ(widget_, widget);
    widget->RemoveObserver(this);
    manager_->OnChromeVoxPanelClosing();
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    CHECK_EQ(widget_, widget);
    widget->RemoveObserver(this);
    manager_->OnChromeVoxPanelDestroying();
  }

 private:
  views::Widget* widget_;
  AccessibilityManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxPanelWidgetObserver);
};

///////////////////////////////////////////////////////////////////////////////
// AccessibilityStatusEventDetails

AccessibilityStatusEventDetails::AccessibilityStatusEventDetails(
    AccessibilityNotificationType notification_type,
    bool enabled,
    ash::AccessibilityNotificationVisibility notify)
    : notification_type(notification_type),
      enabled(enabled),
      magnifier_type(ash::kDefaultMagnifierType),
      notify(notify) {}

AccessibilityStatusEventDetails::AccessibilityStatusEventDetails(
    AccessibilityNotificationType notification_type,
    bool enabled,
    ash::MagnifierType magnifier_type,
    ash::AccessibilityNotificationVisibility notify)
    : notification_type(notification_type),
      enabled(enabled),
      magnifier_type(magnifier_type),
      notify(notify) {}

///////////////////////////////////////////////////////////////////////////////
//
// AccessibilityManager::PrefHandler

AccessibilityManager::PrefHandler::PrefHandler(const char* pref_path)
    : pref_path_(pref_path) {}

AccessibilityManager::PrefHandler::~PrefHandler() {}

void AccessibilityManager::PrefHandler::HandleProfileChanged(
    Profile* previous_profile, Profile* current_profile) {
  // Returns if the current profile is null.
  if (!current_profile)
    return;

  // If the user set a pref value on the login screen and is now starting a
  // session with a new profile, copy the pref value to the profile.
  if ((previous_profile &&
       ProfileHelper::IsSigninProfile(previous_profile) &&
       current_profile->IsNewProfile() &&
       !ProfileHelper::IsSigninProfile(current_profile)) ||
      // Special case for Guest mode:
      // Guest mode launches a guest-mode browser process before session starts,
      // so the previous profile is null.
      (!previous_profile &&
       current_profile->IsGuestSession())) {
    // Returns if the pref has not been set by the user.
    const PrefService::Preference* pref = ProfileHelper::GetSigninProfile()->
        GetPrefs()->FindPreference(pref_path_);
    if (!pref || !pref->IsUserControlled())
      return;

    // Copy the pref value from the signin screen.
    const base::Value* value_on_login = pref->GetValue();
    PrefService* user_prefs = current_profile->GetPrefs();
    user_prefs->Set(pref_path_, *value_on_login);
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// AccessibilityManager

// static
void AccessibilityManager::Initialize() {
  CHECK(g_accessibility_manager == NULL);
  g_accessibility_manager = new AccessibilityManager();
}

// static
void AccessibilityManager::Shutdown() {
  CHECK(g_accessibility_manager);
  delete g_accessibility_manager;
  g_accessibility_manager = NULL;
}

// static
AccessibilityManager* AccessibilityManager::Get() {
  return g_accessibility_manager;
}

AccessibilityManager::AccessibilityManager()
    : profile_(NULL),
      large_cursor_pref_handler_(prefs::kAccessibilityLargeCursorEnabled),
      spoken_feedback_pref_handler_(prefs::kAccessibilitySpokenFeedbackEnabled),
      high_contrast_pref_handler_(prefs::kAccessibilityHighContrastEnabled),
      autoclick_pref_handler_(prefs::kAccessibilityAutoclickEnabled),
      autoclick_delay_pref_handler_(prefs::kAccessibilityAutoclickDelayMs),
      virtual_keyboard_pref_handler_(
          prefs::kAccessibilityVirtualKeyboardEnabled),
      mono_audio_pref_handler_(prefs::kAccessibilityMonoAudioEnabled),
      caret_highlight_pref_handler_(prefs::kAccessibilityCaretHighlightEnabled),
      cursor_highlight_pref_handler_(
          prefs::kAccessibilityCursorHighlightEnabled),
      focus_highlight_pref_handler_(prefs::kAccessibilityFocusHighlightEnabled),
      select_to_speak_pref_handler_(prefs::kAccessibilitySelectToSpeakEnabled),
      switch_access_pref_handler_(prefs::kAccessibilitySwitchAccessEnabled),
      large_cursor_enabled_(false),
      sticky_keys_enabled_(false),
      spoken_feedback_enabled_(false),
      high_contrast_enabled_(false),
      autoclick_enabled_(false),
      autoclick_delay_ms_(ash::AutoclickController::GetDefaultAutoclickDelay()),
      virtual_keyboard_enabled_(false),
      mono_audio_enabled_(false),
      caret_highlight_enabled_(false),
      cursor_highlight_enabled_(false),
      focus_highlight_enabled_(false),
      select_to_speak_enabled_(false),
      switch_access_enabled_(false),
      spoken_feedback_notification_(ash::A11Y_NOTIFICATION_NONE),
      should_speak_chrome_vox_announcements_on_user_screen_(true),
      system_sounds_enabled_(false),
      braille_display_connected_(false),
      scoped_braille_observer_(this),
      braille_ime_current_(false),
      chromevox_panel_(nullptr),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_SESSION_STARTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                              content::NotificationService::AllSources());

  input_method::InputMethodManager::Get()->AddObserver(this);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  media::SoundsManager* manager = media::SoundsManager::Get();
  manager->Initialize(SOUND_SHUTDOWN,
                      bundle.GetRawDataResource(IDR_SOUND_SHUTDOWN_WAV));
  manager->Initialize(
      SOUND_SPOKEN_FEEDBACK_ENABLED,
      bundle.GetRawDataResource(IDR_SOUND_SPOKEN_FEEDBACK_ENABLED_WAV));
  manager->Initialize(
      SOUND_SPOKEN_FEEDBACK_DISABLED,
      bundle.GetRawDataResource(IDR_SOUND_SPOKEN_FEEDBACK_DISABLED_WAV));
  manager->Initialize(SOUND_PASSTHROUGH,
                      bundle.GetRawDataResource(IDR_SOUND_PASSTHROUGH_WAV));
  manager->Initialize(SOUND_EXIT_SCREEN,
                      bundle.GetRawDataResource(IDR_SOUND_EXIT_SCREEN_WAV));
  manager->Initialize(SOUND_ENTER_SCREEN,
                      bundle.GetRawDataResource(IDR_SOUND_ENTER_SCREEN_WAV));

  base::FilePath resources_path;
  if (!PathService::Get(chrome::DIR_RESOURCES, &resources_path))
    NOTREACHED();
  chromevox_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kChromeVoxExtensionId,
      resources_path.Append(extension_misc::kChromeVoxExtensionPath),
      base::Bind(&AccessibilityManager::PostUnloadChromeVox,
                 weak_ptr_factory_.GetWeakPtr())));
  select_to_speak_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kSelectToSpeakExtensionId,
      resources_path.Append(extension_misc::kSelectToSpeakExtensionPath),
      base::Closure()));
}

AccessibilityManager::~AccessibilityManager() {
  CHECK(this == g_accessibility_manager);
  AccessibilityStatusEventDetails details(ACCESSIBILITY_MANAGER_SHUTDOWN, false,
                                          ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
  input_method::InputMethodManager::Get()->RemoveObserver(this);

  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }
}

bool AccessibilityManager::ShouldShowAccessibilityMenu() {
  // If any of the loaded profiles has an accessibility feature turned on - or
  // enforced to always show the menu - we return true to show the menu.
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::iterator it = profiles.begin();
       it != profiles.end();
       ++it) {
    PrefService* pref_service = (*it)->GetPrefs();
    if (pref_service->GetBoolean(prefs::kAccessibilityStickyKeysEnabled) ||
        pref_service->GetBoolean(prefs::kAccessibilityLargeCursorEnabled) ||
        pref_service->GetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled) ||
        pref_service->GetBoolean(prefs::kAccessibilityHighContrastEnabled) ||
        pref_service->GetBoolean(prefs::kAccessibilityAutoclickEnabled) ||
        pref_service->GetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu) ||
        pref_service->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled) ||
        pref_service->GetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled) ||
        pref_service->GetBoolean(prefs::kAccessibilityMonoAudioEnabled))
      return true;
  }
  return false;
}

bool AccessibilityManager::ShouldEnableCursorCompositing() {
  if (!profile_)
    return false;
  PrefService* pref_service = profile_->GetPrefs();
  // Enable cursor compositing when one or more of the listed accessibility
  // features are turned on.
  if (pref_service->GetBoolean(prefs::kAccessibilityLargeCursorEnabled) ||
      pref_service->GetBoolean(prefs::kAccessibilityHighContrastEnabled) ||
      pref_service->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled))
    return true;
  return false;
}

void AccessibilityManager::EnableLargeCursor(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::UpdateLargeCursorFromPref() {
  if (!profile_)
    return;

  const bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityLargeCursorEnabled);

  if (large_cursor_enabled_ == enabled)
    return;

  large_cursor_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_LARGE_CURSOR,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);

  NotifyAccessibilityStatusChanged(details);

  ash::Shell::GetInstance()->cursor_manager()->SetCursorSet(
      enabled ? ui::CURSOR_SET_LARGE : ui::CURSOR_SET_NORMAL);
  ash::Shell::GetInstance()->SetCursorCompositingEnabled(
      ShouldEnableCursorCompositing());
}

bool AccessibilityManager::IsIncognitoAllowed() {
  return profile_ != NULL &&
         profile_->GetProfileType() != Profile::GUEST_PROFILE &&
         IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
             IncognitoModePrefs::DISABLED;
}

bool AccessibilityManager::IsLargeCursorEnabled() {
  return large_cursor_enabled_;
}

void AccessibilityManager::EnableStickyKeys(bool enabled) {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityStickyKeysEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsStickyKeysEnabled() {
  return sticky_keys_enabled_;
}

void AccessibilityManager::UpdateStickyKeysFromPref() {
  if (!profile_)
    return;

  const bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityStickyKeysEnabled);

  if (sticky_keys_enabled_ == enabled)
    return;

  sticky_keys_enabled_ = enabled;
  ash::Shell::GetInstance()->sticky_keys_controller()->Enable(enabled);
}

void AccessibilityManager::EnableSpokenFeedback(
    bool enabled,
    ash::AccessibilityNotificationVisibility notify) {
  if (!profile_)
    return;
  ash::WmShell::Get()->RecordUserMetricsAction(
      enabled ? ash::UMA_STATUS_AREA_ENABLE_SPOKEN_FEEDBACK
              : ash::UMA_STATUS_AREA_DISABLE_SPOKEN_FEEDBACK);

  spoken_feedback_notification_ = notify;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, enabled);
  pref_service->CommitPendingWrite();

  spoken_feedback_notification_ = ash::A11Y_NOTIFICATION_NONE;
}

void AccessibilityManager::UpdateSpokenFeedbackFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilitySpokenFeedbackEnabled);

  if (enabled) {
    chromevox_loader_->SetProfile(
        profile_, base::Bind(&AccessibilityManager::PostSwitchChromeVoxProfile,
                             weak_ptr_factory_.GetWeakPtr()));
  }

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
      enabled,
      spoken_feedback_notification_);

  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    chromevox_loader_->Load(profile_, "window.INJECTED_AFTER_LOAD = true;",
                            base::Bind(&AccessibilityManager::PostLoadChromeVox,
                                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    chromevox_loader_->Unload();
  }
  UpdateBrailleImeState();

  // ChromeVox focus highlighting overrides the other focus highlighting.
  UpdateFocusHighlightFromPref();
}

bool AccessibilityManager::IsSpokenFeedbackEnabled() {
  return spoken_feedback_enabled_;
}

void AccessibilityManager::ToggleSpokenFeedback(
    ash::AccessibilityNotificationVisibility notify) {
  EnableSpokenFeedback(!IsSpokenFeedbackEnabled(), notify);
}

void AccessibilityManager::EnableHighContrast(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityHighContrastEnabled, enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::UpdateHighContrastFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilityHighContrastEnabled);

  if (high_contrast_enabled_ == enabled)
    return;

  high_contrast_enabled_ = enabled;

  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE, enabled,
      ash::A11Y_NOTIFICATION_NONE);

  NotifyAccessibilityStatusChanged(details);

  ash::Shell::GetInstance()->high_contrast_controller()->SetEnabled(enabled);
  ash::Shell::GetInstance()->SetCursorCompositingEnabled(
      ShouldEnableCursorCompositing());
}

void AccessibilityManager::OnLocaleChanged() {
  if (!profile_)
    return;

  if (!IsSpokenFeedbackEnabled())
    return;

  // If the system locale changes and spoken feedback is enabled,
  // reload ChromeVox so that it switches its internal translations
  // to the new language.
  EnableSpokenFeedback(false, ash::A11Y_NOTIFICATION_NONE);
  EnableSpokenFeedback(true, ash::A11Y_NOTIFICATION_NONE);
}

bool AccessibilityManager::PlayEarcon(int sound_key, PlaySoundOption option) {
  DCHECK(sound_key < chromeos::SOUND_COUNT);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kAshDisableSystemSounds))
    return false;
  if (option == PlaySoundOption::SPOKEN_FEEDBACK_ENABLED &&
      !IsSpokenFeedbackEnabled() && !cl->HasSwitch(kAshEnableSystemSounds)) {
    return false;
  }
  return media::SoundsManager::Get()->Play(sound_key);
}

void AccessibilityManager::HandleAccessibilityGesture(ui::AXGesture gesture) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());
  CHECK(event_router);

  std::unique_ptr<base::ListValue> event_args(new base::ListValue());
  event_args->AppendString(ui::ToString(gesture));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_ACCESSIBILITY_GESTURE,
      extensions::api::accessibility_private::OnAccessibilityGesture::
          kEventName,
      std::move(event_args)));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kChromeVoxExtensionId, std::move(event));
}

void AccessibilityManager::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  ash::RootWindowController* root_window_controller =
      ash::RootWindowController::ForTargetRootWindow();
  root_window_controller->SetTouchAccessibilityAnchorPoint(anchor_point);
}

bool AccessibilityManager::IsHighContrastEnabled() {
  return high_contrast_enabled_;
}

void AccessibilityManager::EnableAutoclick(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityAutoclickEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsAutoclickEnabled() {
  return autoclick_enabled_;
}

void AccessibilityManager::UpdateAutoclickFromPref() {
  if (!profile_)
    return;

  bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityAutoclickEnabled);

  if (autoclick_enabled_ == enabled)
    return;
  autoclick_enabled_ = enabled;

  if (chrome::IsRunningInMash()) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    mash::mojom::LaunchablePtr launchable;
    connector->ConnectToInterface("accessibility_autoclick", &launchable);
    launchable->Launch(mash::mojom::kWindow, mash::mojom::LaunchMode::DEFAULT);
    return;
  }

  ash::Shell::GetInstance()->autoclick_controller()->SetEnabled(enabled);
}

void AccessibilityManager::SetAutoclickDelay(int delay_ms) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInteger(prefs::kAccessibilityAutoclickDelayMs, delay_ms);
  pref_service->CommitPendingWrite();
}

int AccessibilityManager::GetAutoclickDelay() const {
  return static_cast<int>(autoclick_delay_ms_.InMilliseconds());
}

void AccessibilityManager::UpdateAutoclickDelayFromPref() {
  if (!profile_)
    return;

  base::TimeDelta autoclick_delay_ms = base::TimeDelta::FromMilliseconds(
      int64_t{profile_->GetPrefs()->GetInteger(
          prefs::kAccessibilityAutoclickDelayMs)});

  if (autoclick_delay_ms == autoclick_delay_ms_)
    return;
  autoclick_delay_ms_ = autoclick_delay_ms;

  if (chrome::IsRunningInMash()) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    ash::autoclick::mojom::AutoclickControllerPtr autoclick_controller;
    connector->ConnectToInterface("accessibility_autoclick",
                                  &autoclick_controller);
    autoclick_controller->SetAutoclickDelay(
        autoclick_delay_ms_.InMilliseconds());
    return;
  }

  ash::Shell::GetInstance()->autoclick_controller()->SetAutoclickDelay(
      autoclick_delay_ms_);
}

void AccessibilityManager::EnableVirtualKeyboard(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsVirtualKeyboardEnabled() {
  return virtual_keyboard_enabled_;
}

void AccessibilityManager::UpdateVirtualKeyboardFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilityVirtualKeyboardEnabled);

  if (virtual_keyboard_enabled_ == enabled)
    return;
  virtual_keyboard_enabled_ = enabled;

  keyboard::SetAccessibilityKeyboardEnabled(enabled);
  if (!chrome::IsRunningInMash()) {
    // Note that there are two versions of the on-screen keyboard. A full layout
    // is provided for accessibility, which includes sticky modifier keys to
    // enable typing of hotkeys. A compact version is used in touchview mode
    // to provide a layout with larger keys to facilitate touch typing. In the
    // event that the a11y keyboard is being disabled, an on-screen keyboard
    // might still be enabled and a forced reset is required to pick up the
    // layout change.
    if (keyboard::IsKeyboardEnabled())
      ash::Shell::GetInstance()->CreateKeyboard();
    else
      ash::Shell::GetInstance()->DeactivateKeyboard();
  } else {
    // TODO(mash): Support on-screen keyboard. See http://crbug.com/646565
    NOTIMPLEMENTED();
  }

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::EnableMonoAudio(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityMonoAudioEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsMonoAudioEnabled() {
  return mono_audio_enabled_;
}

void AccessibilityManager::UpdateMonoAudioFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilityMonoAudioEnabled);

  if (mono_audio_enabled_ == enabled)
    return;
  mono_audio_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_MONO_AUDIO,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);

  ash::Shell::GetInstance()->audio_a11y_controller()->SetOutputMono(enabled);
}

void AccessibilityManager::SetCaretHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsCaretHighlightEnabled() const {
  return caret_highlight_enabled_;
}

void AccessibilityManager::UpdateCaretHighlightFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilityCaretHighlightEnabled);

  if (caret_highlight_enabled_ == enabled)
    return;
  caret_highlight_enabled_ = enabled;

  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityManager::SetCursorHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityCursorHighlightEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsCursorHighlightEnabled() const {
  return cursor_highlight_enabled_;
}

void AccessibilityManager::UpdateCursorHighlightFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilityCursorHighlightEnabled);

  if (cursor_highlight_enabled_ == enabled)
    return;
  cursor_highlight_enabled_ = enabled;

  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityManager::SetFocusHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityFocusHighlightEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsFocusHighlightEnabled() const {
  return focus_highlight_enabled_;
}

void AccessibilityManager::UpdateFocusHighlightFromPref() {
  if (!profile_)
    return;

  bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilityFocusHighlightEnabled);

  // Focus highlighting can't be on when spoken feedback is on, because
  // ChromeVox does its own focus highlighting.
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kAccessibilitySpokenFeedbackEnabled))
    enabled = false;

  if (focus_highlight_enabled_ == enabled)
    return;
  focus_highlight_enabled_ = enabled;

  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityManager::SetSelectToSpeakEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilitySelectToSpeakEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsSelectToSpeakEnabled() const {
  return select_to_speak_enabled_;
}

void AccessibilityManager::UpdateSelectToSpeakFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilitySelectToSpeakEnabled);

  if (select_to_speak_enabled_ == enabled)
    return;
  select_to_speak_enabled_ = enabled;

  if (enabled) {
    select_to_speak_loader_->Load(profile_, "" /* init_script_str */,
                                  base::Closure() /* done_cb */);
    select_to_speak_event_handler_.reset(
        new chromeos::SelectToSpeakEventHandler());
  } else {
    select_to_speak_loader_->Unload();
    select_to_speak_event_handler_.reset(nullptr);
  }
}

void AccessibilityManager::SetSwitchAccessEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilitySwitchAccessEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsSwitchAccessEnabled() const {
  return switch_access_enabled_;
}

void AccessibilityManager::UpdateSwitchAccessFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilitySwitchAccessEnabled);

  if (switch_access_enabled_ == enabled)
    return;
  switch_access_enabled_ = enabled;

  // TODO(dmazzoni): implement feature here.
}

void AccessibilityManager::UpdateAccessibilityHighlightingFromPrefs() {
  if (!focus_highlight_enabled_ && !caret_highlight_enabled_ &&
      !cursor_highlight_enabled_) {
    if (accessibility_highlight_manager_)
      accessibility_highlight_manager_.reset();
    return;
  }

  if (!accessibility_highlight_manager_) {
    accessibility_highlight_manager_.reset(new AccessibilityHighlightManager());
    accessibility_highlight_manager_->RegisterObservers();
  }

  accessibility_highlight_manager_->HighlightFocus(focus_highlight_enabled_);
  accessibility_highlight_manager_->HighlightCaret(caret_highlight_enabled_);
  accessibility_highlight_manager_->HighlightCursor(cursor_highlight_enabled_);
}

bool AccessibilityManager::IsBrailleDisplayConnected() const {
  return braille_display_connected_;
}

void AccessibilityManager::CheckBrailleState() {
  BrailleController* braille_controller = GetBrailleController();
  if (!scoped_braille_observer_.IsObserving(braille_controller))
    scoped_braille_observer_.Add(braille_controller);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&BrailleController::GetDisplayState,
                 base::Unretained(braille_controller)),
      base::Bind(&AccessibilityManager::ReceiveBrailleDisplayState,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AccessibilityManager::ReceiveBrailleDisplayState(
    std::unique_ptr<extensions::api::braille_display_private::DisplayState>
        state) {
  OnBrailleDisplayStateChanged(*state);
}

void AccessibilityManager::UpdateBrailleImeState() {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  std::vector<std::string> preload_engines =
      base::SplitString(pref_service->GetString(prefs::kLanguagePreloadEngines),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<std::string>::iterator it =
      std::find(preload_engines.begin(),
                preload_engines.end(),
                extension_misc::kBrailleImeEngineId);
  bool is_enabled = (it != preload_engines.end());
  bool should_be_enabled =
      (spoken_feedback_enabled_ && braille_display_connected_);
  if (is_enabled == should_be_enabled)
    return;
  if (should_be_enabled)
    preload_engines.push_back(extension_misc::kBrailleImeEngineId);
  else
    preload_engines.erase(it);
  pref_service->SetString(prefs::kLanguagePreloadEngines,
                          base::JoinString(preload_engines, ","));
  braille_ime_current_ = false;
}

// Overridden from InputMethodManager::Observer.
void AccessibilityManager::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* /* profile */,
    bool show_message) {
  // Sticky keys is implemented only in ash.
  // TODO(dpolukhin): support Athena, crbug.com/408733.
  if (!chrome::IsRunningInMash()) {
    ash::Shell::GetInstance()->sticky_keys_controller()->SetModifiersEnabled(
        manager->IsISOLevel5ShiftUsedByCurrentInputMethod(),
        manager->IsAltGrUsedByCurrentInputMethod());
  }
  const chromeos::input_method::InputMethodDescriptor descriptor =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  braille_ime_current_ =
      (descriptor.id() == extension_misc::kBrailleImeEngineId);
}

void AccessibilityManager::SetProfile(Profile* profile) {
  pref_change_registrar_.reset();
  local_state_pref_change_registrar_.reset();

  if (profile) {
    // TODO(yoshiki): Move following code to PrefHandler.
    pref_change_registrar_.reset(new PrefChangeRegistrar);
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        prefs::kAccessibilityLargeCursorEnabled,
        base::Bind(&AccessibilityManager::UpdateLargeCursorFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityStickyKeysEnabled,
        base::Bind(&AccessibilityManager::UpdateStickyKeysFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilitySpokenFeedbackEnabled,
        base::Bind(&AccessibilityManager::UpdateSpokenFeedbackFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityHighContrastEnabled,
        base::Bind(&AccessibilityManager::UpdateHighContrastFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityAutoclickEnabled,
        base::Bind(&AccessibilityManager::UpdateAutoclickFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityAutoclickDelayMs,
        base::Bind(&AccessibilityManager::UpdateAutoclickDelayFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityVirtualKeyboardEnabled,
        base::Bind(&AccessibilityManager::UpdateVirtualKeyboardFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityMonoAudioEnabled,
        base::Bind(&AccessibilityManager::UpdateMonoAudioFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityCaretHighlightEnabled,
        base::Bind(&AccessibilityManager::UpdateCaretHighlightFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityCursorHighlightEnabled,
        base::Bind(&AccessibilityManager::UpdateCursorHighlightFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityFocusHighlightEnabled,
        base::Bind(&AccessibilityManager::UpdateFocusHighlightFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilitySelectToSpeakEnabled,
        base::Bind(&AccessibilityManager::UpdateSelectToSpeakFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilitySwitchAccessEnabled,
        base::Bind(&AccessibilityManager::UpdateSwitchAccessFromPref,
                   base::Unretained(this)));

    local_state_pref_change_registrar_.reset(new PrefChangeRegistrar);
    local_state_pref_change_registrar_->Init(g_browser_process->local_state());
    local_state_pref_change_registrar_->Add(
        prefs::kApplicationLocale,
        base::Bind(&AccessibilityManager::OnLocaleChanged,
                   base::Unretained(this)));

    content::BrowserAccessibilityState::GetInstance()->AddHistogramCallback(
        base::Bind(
            &AccessibilityManager::UpdateChromeOSAccessibilityHistograms,
            base::Unretained(this)));

    chromevox_loader_->SetProfile(
        profile, base::Bind(&AccessibilityManager::PostSwitchChromeVoxProfile,
                            weak_ptr_factory_.GetWeakPtr()));

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    if (!extension_registry_observer_.IsObserving(registry))
      extension_registry_observer_.Add(registry);
  }

  large_cursor_pref_handler_.HandleProfileChanged(profile_, profile);
  spoken_feedback_pref_handler_.HandleProfileChanged(profile_, profile);
  high_contrast_pref_handler_.HandleProfileChanged(profile_, profile);
  autoclick_pref_handler_.HandleProfileChanged(profile_, profile);
  autoclick_delay_pref_handler_.HandleProfileChanged(profile_, profile);
  virtual_keyboard_pref_handler_.HandleProfileChanged(profile_, profile);
  mono_audio_pref_handler_.HandleProfileChanged(profile_, profile);
  caret_highlight_pref_handler_.HandleProfileChanged(profile_, profile);
  cursor_highlight_pref_handler_.HandleProfileChanged(profile_, profile);
  focus_highlight_pref_handler_.HandleProfileChanged(profile_, profile);
  select_to_speak_pref_handler_.HandleProfileChanged(profile_, profile);
  switch_access_pref_handler_.HandleProfileChanged(profile_, profile);

  bool had_profile = (profile_ != NULL);
  profile_ = profile;

  if (!had_profile && profile)
    CheckBrailleState();
  else
    UpdateBrailleImeState();
  UpdateLargeCursorFromPref();
  UpdateStickyKeysFromPref();
  UpdateSpokenFeedbackFromPref();
  UpdateHighContrastFromPref();
  UpdateAutoclickFromPref();
  UpdateAutoclickDelayFromPref();
  UpdateVirtualKeyboardFromPref();
  UpdateMonoAudioFromPref();
  UpdateCaretHighlightFromPref();
  UpdateCursorHighlightFromPref();
  UpdateFocusHighlightFromPref();
  UpdateSelectToSpeakFromPref();
  UpdateSwitchAccessFromPref();

  // Update the panel height in the shelf layout manager when the profile
  // changes, since the shelf layout manager doesn't exist in the login profile.
  if (chromevox_panel_)
    chromevox_panel_->UpdatePanelHeight();
}

void AccessibilityManager::ActiveUserChanged(const AccountId& account_id) {
  SetProfile(ProfileManager::GetActiveUserProfile());
}

void AccessibilityManager::OnAppTerminating() {
  session_state_observer_.reset();
}

void AccessibilityManager::OnFullscreenStateChanged(
    bool is_fullscreen,
    ash::WmWindow* root_window) {
  if (chromevox_panel_)
    chromevox_panel_->UpdateWidgetBounds();
}

void AccessibilityManager::SetProfileForTest(Profile* profile) {
  SetProfile(profile);
}

void AccessibilityManager::SetBrailleControllerForTest(
    BrailleController* controller) {
  g_braille_controller_for_test = controller;
}

void AccessibilityManager::EnableSystemSounds(bool system_sounds_enabled) {
  system_sounds_enabled_ = system_sounds_enabled;
}

base::TimeDelta AccessibilityManager::PlayShutdownSound() {
  if (!system_sounds_enabled_)
    return base::TimeDelta();
  system_sounds_enabled_ = false;
  if (!PlayEarcon(SOUND_SHUTDOWN, PlaySoundOption::SPOKEN_FEEDBACK_ENABLED))
    return base::TimeDelta();
  return media::SoundsManager::Get()->GetDuration(SOUND_SHUTDOWN);
}

void AccessibilityManager::InjectChromeVox(RenderViewHost* render_view_host) {
  chromevox_loader_->LoadExtension(profile_, render_view_host, base::Closure());
}

std::unique_ptr<AccessibilityStatusSubscription>
AccessibilityManager::RegisterCallback(const AccessibilityStatusCallback& cb) {
  return callback_list_.Add(cb);
}

void AccessibilityManager::NotifyAccessibilityStatusChanged(
    AccessibilityStatusEventDetails& details) {
  callback_list_.Notify(details);
}

void AccessibilityManager::UpdateChromeOSAccessibilityHistograms() {
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosSpokenFeedback",
                        IsSpokenFeedbackEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosHighContrast",
                        IsHighContrastEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosVirtualKeyboard",
                        IsVirtualKeyboardEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosStickyKeys", IsStickyKeysEnabled());
  if (MagnificationManager::Get()) {
    uint32_t type = MagnificationManager::Get()->IsMagnifierEnabled()
                        ? MagnificationManager::Get()->GetMagnifierType()
                        : 0;
    // '0' means magnifier is disabled.
    UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosScreenMagnifier", type,
                              ash::kMaxMagnifierType + 1);
  }
  if (profile_) {
    const PrefService* const prefs = profile_->GetPrefs();
    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.CrosLargeCursor",
        prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled));
    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.CrosAlwaysShowA11yMenu",
        prefs->GetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu));

    bool autoclick_enabled =
        prefs->GetBoolean(prefs::kAccessibilityAutoclickEnabled);
    UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosAutoclick", autoclick_enabled);
    if (autoclick_enabled) {
      // We only want to log the autoclick delay if the user has actually
      // enabled autoclick.
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Accessibility.CrosAutoclickDelay",
          base::TimeDelta::FromMilliseconds(
              prefs->GetInteger(prefs::kAccessibilityAutoclickDelayMs)),
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMilliseconds(3000),
          50);
    }
  }
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosCaretHighlight",
                        IsCaretHighlightEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosCursorHighlight",
                        IsCursorHighlightEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosFocusHighlight",
                        IsFocusHighlightEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosSelectToSpeak",
                        IsSelectToSpeakEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosSwitchAccess",
                        IsSwitchAccessEnabled());
}

void AccessibilityManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
      // Update |profile_| when entering the login screen.
      Profile* profile = ProfileManager::GetActiveUserProfile();
      if (ProfileHelper::IsSigninProfile(profile))
        SetProfile(profile);
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED:
      // Update |profile_| when entering a session.
      SetProfile(ProfileManager::GetActiveUserProfile());

      // Ensure ChromeVox makes announcements at the start of new sessions.
      should_speak_chrome_vox_announcements_on_user_screen_ = true;

      // Add a session state observer to be able to monitor session changes.
      if (!session_state_observer_.get() && ash::Shell::HasInstance())
        session_state_observer_.reset(
            new ash::ScopedSessionStateObserver(this));
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Update |profile_| when exiting a session or shutting down.
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile_ == profile)
        SetProfile(NULL);
      break;
    }
    case chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED: {
      bool is_screen_locked = *content::Details<bool>(details).ptr();
      if (spoken_feedback_enabled_) {
        if (is_screen_locked)
          chromevox_loader_->LoadToLockScreen(base::Closure());
        // If spoken feedback was enabled, make sure it is also enabled on
        // the user screen.
        // The status tray gets verbalized by user screen ChromeVox, so we need
        // to load it on the user screen even if the screen is locked.
        chromevox_loader_->LoadToUserScreen(base::Closure());
      }
      break;
    }
  }
}

void AccessibilityManager::OnBrailleDisplayStateChanged(
    const DisplayState& display_state) {
  braille_display_connected_ = display_state.available;
  if (braille_display_connected_) {
    EnableSpokenFeedback(true, ash::A11Y_NOTIFICATION_SHOW);
  }
  UpdateBrailleImeState();

  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_BRAILLE_DISPLAY_CONNECTION_STATE_CHANGED,
      braille_display_connected_, ash::A11Y_NOTIFICATION_SHOW);
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::OnBrailleKeyEvent(const KeyEvent& event) {
  // Ensure the braille IME is active on braille keyboard (dots) input.
  if ((event.command ==
       extensions::api::braille_display_private::KEY_COMMAND_DOTS) &&
      !braille_ime_current_) {
    input_method::InputMethodManager::Get()
        ->GetActiveIMEState()
        ->ChangeInputMethod(extension_misc::kBrailleImeEngineId,
                            false /* show_message */);
  }
}

void AccessibilityManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (extension->id() == keyboard_listener_extension_id_) {
    keyboard_listener_extension_id_ = std::string();
    keyboard_listener_capture_ = false;
  }
}

void AccessibilityManager::OnShutdown(extensions::ExtensionRegistry* registry) {
  extension_registry_observer_.Remove(registry);
}

void AccessibilityManager::PostLoadChromeVox() {
  // Do any setup work needed immediately after ChromeVox actually loads.
  PlayEarcon(SOUND_SPOKEN_FEEDBACK_ENABLED, PlaySoundOption::ALWAYS);

  if (chromevox_loader_->loaded_on_lock_screen() ||
      should_speak_chrome_vox_announcements_on_user_screen_) {
    extensions::EventRouter* event_router =
        extensions::EventRouter::Get(profile_);
    CHECK(event_router);

    std::unique_ptr<base::ListValue> event_args(new base::ListValue());
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::ACCESSIBILITY_PRIVATE_ON_INTRODUCE_CHROME_VOX,
        extensions::api::accessibility_private::OnIntroduceChromeVox::
            kEventName,
        std::move(event_args)));
    event_router->DispatchEventWithLazyListener(
        extension_misc::kChromeVoxExtensionId, std::move(event));
  }

  should_speak_chrome_vox_announcements_on_user_screen_ =
      chromevox_loader_->loaded_on_lock_screen();

  if (!chromevox_panel_) {
    chromevox_panel_ = new ChromeVoxPanel(profile_);
    chromevox_panel_widget_observer_.reset(
        new ChromeVoxPanelWidgetObserver(chromevox_panel_->GetWidget(), this));
  }
}

void AccessibilityManager::PostUnloadChromeVox() {
  // Do any teardown work needed immediately after ChromeVox actually unloads.
  PlayEarcon(SOUND_SPOKEN_FEEDBACK_DISABLED, PlaySoundOption::ALWAYS);
  // Clear the accessibility focus ring.
  AccessibilityFocusRingController::GetInstance()->SetFocusRing(
      std::vector<gfx::Rect>(),
      AccessibilityFocusRingController::PERSIST_FOCUS_RING);

  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }
}

void AccessibilityManager::PostSwitchChromeVoxProfile() {
  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }
  chromevox_panel_ = new ChromeVoxPanel(profile_);
  chromevox_panel_widget_observer_.reset(
      new ChromeVoxPanelWidgetObserver(chromevox_panel_->GetWidget(), this));
}

void AccessibilityManager::OnChromeVoxPanelClosing() {
  aura::Window* root_window = chromevox_panel_->GetRootWindow();
  chromevox_panel_widget_observer_.reset(nullptr);
  chromevox_panel_ = nullptr;

  ash::WmShelf* shelf =
      ash::WmShelf::ForWindow(ash::WmWindowAura::Get(root_window));
  if (!shelf->IsShelfInitialized())
    return;

  ash::ShelfLayoutManager* shelf_layout_manager = shelf->shelf_layout_manager();
  if (shelf_layout_manager)
    shelf_layout_manager->SetChromeVoxPanelHeight(0);
}

void AccessibilityManager::OnChromeVoxPanelDestroying() {
  chromevox_panel_widget_observer_.reset(nullptr);
  chromevox_panel_ = nullptr;
}

void AccessibilityManager::SetKeyboardListenerExtensionId(
    const std::string& id,
    content::BrowserContext* context) {
  keyboard_listener_extension_id_ = id;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);
  if (!extension_registry_observer_.IsObserving(registry) && !id.empty())
    extension_registry_observer_.Add(registry);
}

}  // namespace chromeos

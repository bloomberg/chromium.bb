// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/autoclick/mus/public/interfaces/autoclick.mojom.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
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
#include "chrome/browser/chromeos/accessibility/switch_access_event_handler.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/audio/audio_a11y_controller.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
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
#include "media/base/media_switches.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using content::BrowserThread;
using extensions::api::braille_display_private::BrailleController;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::StubBrailleController;

namespace chromeos {

namespace {

// When this flag is set, system sounds will not be played.
const char kAshDisableSystemSounds[] = "ash-disable-system-sounds";

static chromeos::AccessibilityManager* g_accessibility_manager = NULL;

static BrailleController* g_braille_controller_for_test = NULL;

BrailleController* GetBrailleController() {
  if (g_braille_controller_for_test)
    return g_braille_controller_for_test;
  // Don't use the real braille controller for tests to avoid automatically
  // starting ChromeVox which confuses some tests.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kTestType))
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
    : notification_type(notification_type), enabled(enabled), notify(notify) {}

///////////////////////////////////////////////////////////////////////////////
// AccessibilityManager::PrefHandler

AccessibilityManager::PrefHandler::PrefHandler(const char* pref_path)
    : pref_path_(pref_path) {}

AccessibilityManager::PrefHandler::~PrefHandler() {}

void AccessibilityManager::PrefHandler::HandleProfileChanged(
    Profile* previous_profile,
    Profile* current_profile) {
  // Returns if the current profile is null.
  if (!current_profile)
    return;

  // If the user set a pref value on the login screen and is now starting a
  // session with a new profile, copy the pref value to the profile.
  if ((previous_profile && ProfileHelper::IsSigninProfile(previous_profile) &&
       current_profile->IsNewProfile() &&
       !ProfileHelper::IsSigninProfile(current_profile)) ||
      // Special case for Guest mode:
      // Guest mode launches a guest-mode browser process before session starts,
      // so the previous profile is null.
      (!previous_profile && current_profile->IsGuestSession())) {
    // Returns if the pref has not been set by the user.
    const PrefService::Preference* pref =
        ProfileHelper::GetSigninProfile()->GetPrefs()->FindPreference(
            pref_path_);
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
      large_cursor_pref_handler_(ash::prefs::kAccessibilityLargeCursorEnabled),
      sticky_keys_pref_handler_(ash::prefs::kAccessibilityStickyKeysEnabled),
      spoken_feedback_pref_handler_(
          ash::prefs::kAccessibilitySpokenFeedbackEnabled),
      high_contrast_pref_handler_(
          ash::prefs::kAccessibilityHighContrastEnabled),
      autoclick_pref_handler_(ash::prefs::kAccessibilityAutoclickEnabled),
      autoclick_delay_pref_handler_(ash::prefs::kAccessibilityAutoclickDelayMs),
      virtual_keyboard_pref_handler_(
          ash::prefs::kAccessibilityVirtualKeyboardEnabled),
      mono_audio_pref_handler_(ash::prefs::kAccessibilityMonoAudioEnabled),
      caret_highlight_pref_handler_(
          ash::prefs::kAccessibilityCaretHighlightEnabled),
      cursor_highlight_pref_handler_(
          ash::prefs::kAccessibilityCursorHighlightEnabled),
      focus_highlight_pref_handler_(
          ash::prefs::kAccessibilityFocusHighlightEnabled),
      tap_dragging_pref_handler_(prefs::kTapDraggingEnabled),
      select_to_speak_pref_handler_(
          ash::prefs::kAccessibilitySelectToSpeakEnabled),
      switch_access_pref_handler_(
          ash::prefs::kAccessibilitySwitchAccessEnabled),
      sticky_keys_enabled_(false),
      spoken_feedback_enabled_(false),
      autoclick_enabled_(false),
      autoclick_delay_ms_(ash::AutoclickController::GetDefaultAutoclickDelay()),
      virtual_keyboard_enabled_(false),
      mono_audio_enabled_(false),
      caret_highlight_enabled_(false),
      cursor_highlight_enabled_(false),
      focus_highlight_enabled_(false),
      tap_dragging_enabled_(false),
      select_to_speak_enabled_(false),
      switch_access_enabled_(false),
      spoken_feedback_notification_(ash::A11Y_NOTIFICATION_NONE),
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
                              chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());

  input_method::InputMethodManager::Get()->AddObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);

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
  manager->Initialize(SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_HIGH,
                      bundle.GetRawDataResource(
                          IDR_SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_HIGH_WAV));
  manager->Initialize(SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_LOW,
                      bundle.GetRawDataResource(
                          IDR_SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_LOW_WAV));
  manager->Initialize(SOUND_TOUCH_TYPE,
                      bundle.GetRawDataResource(IDR_SOUND_TOUCH_TYPE_WAV));

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
  switch_access_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kSwitchAccessExtensionId,
      resources_path.Append(extension_misc::kSwitchAccessExtensionPath),
      base::Closure()));
}

AccessibilityManager::~AccessibilityManager() {
  CHECK(this == g_accessibility_manager);
  AccessibilityStatusEventDetails details(ACCESSIBILITY_MANAGER_SHUTDOWN, false,
                                          ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
  input_method::InputMethodManager::Get()->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);

  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }
}

bool AccessibilityManager::ShouldShowAccessibilityMenu() {
  // If any of the loaded profiles has an accessibility feature turned on - or
  // enforced to always show the menu - we return true to show the menu.
  // NOTE: This includes the login screen profile, so if a feature is turned on
  // at the login screen the menu will show even if the user has no features
  // enabled inside the session. http://crbug.com/755631
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::iterator it = profiles.begin();
       it != profiles.end(); ++it) {
    PrefService* prefs = (*it)->GetPrefs();
    if (prefs->GetBoolean(ash::prefs::kAccessibilityStickyKeysEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityHighContrastEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled) ||
        prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityVirtualKeyboardEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityMonoAudioEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityCaretHighlightEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityCursorHighlightEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled) ||
        prefs->GetBoolean(prefs::kTapDraggingEnabled))
      return true;
  }
  return false;
}

void AccessibilityManager::UpdateAlwaysShowMenuFromPref() {
  if (!profile_)
    return;

  // TODO(crbug.com/594887): Fix for mash by moving pref into ash.
  if (GetAshConfig() == ash::Config::MASH)
    return;

  // Update system tray menu visibility.
  ash::Shell::Get()->system_tray_notifier()->NotifyAccessibilityStatusChanged(
      ash::A11Y_NOTIFICATION_NONE);
}

void AccessibilityManager::EnableLargeCursor(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::OnLargeCursorChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_LARGE_CURSOR,
                                          IsLargeCursorEnabled(),
                                          ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
}

bool AccessibilityManager::IsLargeCursorEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityLargeCursorEnabled);
}

void AccessibilityManager::EnableStickyKeys(bool enabled) {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityStickyKeysEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsStickyKeysEnabled() const {
  return sticky_keys_enabled_;
}

void AccessibilityManager::UpdateStickyKeysFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityStickyKeysEnabled);

  if (sticky_keys_enabled_ == enabled)
    return;

  sticky_keys_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_STICKY_KEYS,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);

  // TODO(crbug.com/594887): Fix for mash by moving pref into ash.
  if (GetAshConfig() == ash::Config::MASH)
    return;

  ash::Shell::Get()->sticky_keys_controller()->Enable(enabled);
}

void AccessibilityManager::EnableSpokenFeedback(
    bool enabled,
    ash::AccessibilityNotificationVisibility notify) {
  if (!profile_)
    return;

  spoken_feedback_notification_ = notify;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                           enabled);
  pref_service->CommitPendingWrite();

  spoken_feedback_notification_ = ash::A11Y_NOTIFICATION_NONE;
}

void AccessibilityManager::UpdateSpokenFeedbackFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  if (enabled) {
    chromevox_loader_->SetProfile(
        profile_, base::Bind(&AccessibilityManager::PostSwitchChromeVoxProfile,
                             weak_ptr_factory_.GetWeakPtr()));
  }

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
                                          enabled,
                                          spoken_feedback_notification_);

  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    chromevox_loader_->Load(profile_,
                            base::Bind(&AccessibilityManager::PostLoadChromeVox,
                                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    chromevox_loader_->Unload();
  }
  UpdateBrailleImeState();

  // ChromeVox focus highlighting overrides the other focus highlighting.
  UpdateFocusHighlightFromPref();
}

bool AccessibilityManager::IsSpokenFeedbackEnabled() const {
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
  pref_service->SetBoolean(ash::prefs::kAccessibilityHighContrastEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsHighContrastEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityHighContrastEnabled);
}

void AccessibilityManager::OnHighContrastChanged() {
  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE, IsHighContrastEnabled(),
      ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
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

void AccessibilityManager::OnViewFocusedInArc(
    const gfx::Rect& bounds_in_screen) {
  accessibility_highlight_manager_->OnViewFocusedInArc(bounds_in_screen);
}

bool AccessibilityManager::PlayEarcon(int sound_key, PlaySoundOption option) {
  DCHECK(sound_key < chromeos::SOUND_COUNT);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kAshDisableSystemSounds))
    return false;
  if (option == PlaySoundOption::SPOKEN_FEEDBACK_ENABLED &&
      !IsSpokenFeedbackEnabled()) {
    return false;
  }
  return media::SoundsManager::Get()->Play(sound_key);
}

void AccessibilityManager::OnTwoFingerTouchStart() {
  if (!profile())
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());
  CHECK(event_router);

  auto event_args = base::MakeUnique<base::ListValue>();
  auto event = base::MakeUnique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_TWO_FINGER_TOUCH_START,
      extensions::api::accessibility_private::OnTwoFingerTouchStart::kEventName,
      std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

void AccessibilityManager::OnTwoFingerTouchStop() {
  if (!profile())
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());
  CHECK(event_router);

  auto event_args = base::MakeUnique<base::ListValue>();
  auto event = base::MakeUnique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_TWO_FINGER_TOUCH_STOP,
      extensions::api::accessibility_private::OnTwoFingerTouchStop::kEventName,
      std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

bool AccessibilityManager::ShouldToggleSpokenFeedbackViaTouch() {
#if 1
  // Temporarily disabling this feature until UI feedback is fixed.
  // http://crbug.com/662501
  return false;
#else
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector)
    return false;

  if (!connector->IsEnterpriseManaged())
    return false;

  const policy::DeviceCloudPolicyManagerChromeOS* const
      device_cloud_policy_manager = connector->GetDeviceCloudPolicyManager();
  if (!device_cloud_policy_manager)
    return false;

  if (!device_cloud_policy_manager->IsRemoraRequisition())
    return false;

  KioskAppManager* manager = KioskAppManager::Get();
  KioskAppManager::App app;
  CHECK(manager->GetApp(manager->GetAutoLaunchApp(), &app));
  return app.was_auto_launched_with_zero_delay;
#endif
}

bool AccessibilityManager::PlaySpokenFeedbackToggleCountdown(int tick_count) {
  return media::SoundsManager::Get()->Play(
      tick_count % 2 ? SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_HIGH
                     : SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_LOW);
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
  for (auto* rwc : ash::RootWindowController::root_window_controllers())
    rwc->SetTouchAccessibilityAnchorPoint(anchor_point);
}

void AccessibilityManager::EnableAutoclick(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityAutoclickEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsAutoclickEnabled() const {
  return autoclick_enabled_;
}

void AccessibilityManager::UpdateAutoclickFromPref() {
  if (!profile_)
    return;

  bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityAutoclickEnabled);

  if (autoclick_enabled_ == enabled)
    return;
  autoclick_enabled_ = enabled;

  if (GetAshConfig() == ash::Config::MASH) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    mash::mojom::LaunchablePtr launchable;
    connector->BindInterface("accessibility_autoclick", &launchable);
    launchable->Launch(mash::mojom::kWindow, mash::mojom::LaunchMode::DEFAULT);
    return;
  }

  ash::Shell::Get()->system_tray_notifier()->NotifyAccessibilityStatusChanged(
      ash::A11Y_NOTIFICATION_NONE);
  ash::Shell::Get()->autoclick_controller()->SetEnabled(enabled);
}

void AccessibilityManager::SetAutoclickDelay(int delay_ms) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInteger(ash::prefs::kAccessibilityAutoclickDelayMs,
                           delay_ms);
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
          ash::prefs::kAccessibilityAutoclickDelayMs)});

  if (autoclick_delay_ms == autoclick_delay_ms_)
    return;
  autoclick_delay_ms_ = autoclick_delay_ms;

  if (GetAshConfig() == ash::Config::MASH) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    ash::autoclick::mojom::AutoclickControllerPtr autoclick_controller;
    connector->BindInterface("accessibility_autoclick", &autoclick_controller);
    autoclick_controller->SetAutoclickDelay(
        autoclick_delay_ms_.InMilliseconds());
    return;
  }

  ash::Shell::Get()->autoclick_controller()->SetAutoclickDelay(
      autoclick_delay_ms_);
}

void AccessibilityManager::EnableVirtualKeyboard(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityVirtualKeyboardEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsVirtualKeyboardEnabled() const {
  return virtual_keyboard_enabled_;
}

void AccessibilityManager::UpdateVirtualKeyboardFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled);

  if (virtual_keyboard_enabled_ == enabled)
    return;
  virtual_keyboard_enabled_ = enabled;

  keyboard::SetAccessibilityKeyboardEnabled(enabled);
  if (GetAshConfig() != ash::Config::MASH) {
    // Note that there are two versions of the on-screen keyboard. A full layout
    // is provided for accessibility, which includes sticky modifier keys to
    // enable typing of hotkeys. A compact version is used in tablet mode
    // to provide a layout with larger keys to facilitate touch typing. In the
    // event that the a11y keyboard is being disabled, an on-screen keyboard
    // might still be enabled and a forced reset is required to pick up the
    // layout change.
    if (keyboard::IsKeyboardEnabled())
      ash::Shell::Get()->CreateKeyboard();
    else
      ash::Shell::Get()->DestroyKeyboard();
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
  pref_service->SetBoolean(ash::prefs::kAccessibilityMonoAudioEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsMonoAudioEnabled() const {
  return mono_audio_enabled_;
}

void AccessibilityManager::UpdateMonoAudioFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityMonoAudioEnabled);

  if (mono_audio_enabled_ == enabled)
    return;
  mono_audio_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_MONO_AUDIO,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);

  // TODO(crbug.com/594887): Fix for mash by moving pref into ash.
  if (GetAshConfig() == ash::Config::MASH)
    return;

  ash::Shell::Get()->audio_a11y_controller()->SetOutputMono(enabled);
}

void AccessibilityManager::SetCaretHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityCaretHighlightEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsCaretHighlightEnabled() const {
  return caret_highlight_enabled_;
}

void AccessibilityManager::UpdateCaretHighlightFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityCaretHighlightEnabled);

  if (caret_highlight_enabled_ == enabled)
    return;
  caret_highlight_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_CARET_HIGHLIGHT,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityManager::SetCursorHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityCursorHighlightEnabled,
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
      ash::prefs::kAccessibilityCursorHighlightEnabled);

  if (cursor_highlight_enabled_ == enabled)
    return;
  cursor_highlight_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_CURSOR_HIGHLIGHT,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityManager::SetFocusHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsFocusHighlightEnabled() const {
  return focus_highlight_enabled_;
}

void AccessibilityManager::UpdateFocusHighlightFromPref() {
  if (!profile_)
    return;

  bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityFocusHighlightEnabled);

  // Focus highlighting can't be on when spoken feedback is on, because
  // ChromeVox does its own focus highlighting.
  if (profile_->GetPrefs()->GetBoolean(
          ash::prefs::kAccessibilitySpokenFeedbackEnabled))
    enabled = false;

  if (focus_highlight_enabled_ == enabled)
    return;
  focus_highlight_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_FOCUS_HIGHLIGHT,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityManager::EnableTapDragging(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kTapDraggingEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsTapDraggingEnabled() const {
  return tap_dragging_enabled_;
}

void AccessibilityManager::UpdateTapDraggingFromPref() {
  if (!profile_)
    return;

  const bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kTapDraggingEnabled);

  if (tap_dragging_enabled_ == enabled)
    return;
  tap_dragging_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_TAP_DRAGGING,
                                          enabled, ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);

  system::TouchpadSettings touchpad_settings;
  touchpad_settings.SetTapDragging(enabled);
}

void AccessibilityManager::SetSelectToSpeakEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilitySelectToSpeakEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsSelectToSpeakEnabled() const {
  return select_to_speak_enabled_;
}

void AccessibilityManager::UpdateSelectToSpeakFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySelectToSpeakEnabled);

  if (select_to_speak_enabled_ == enabled)
    return;
  select_to_speak_enabled_ = enabled;

  if (enabled) {
    select_to_speak_loader_->Load(profile_, base::Closure() /* done_cb */);
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
  pref_service->SetBoolean(ash::prefs::kAccessibilitySwitchAccessEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsSwitchAccessEnabled() const {
  return switch_access_enabled_;
}

void AccessibilityManager::UpdateSwitchAccessFromPref() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySwitchAccessEnabled);

  // The Switch Access setting is behind a flag. Don't enable the feature
  // even if the preference is enabled, if the flag isn't also set.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          chromeos::switches::kEnableExperimentalAccessibilityFeatures)) {
    if (enabled) {
      LOG(WARNING) << "Switch access enabled but experimental accessibility "
                   << "features flag is not set.";
    }
    return;
  }

  if (switch_access_enabled_ == enabled)
    return;
  switch_access_enabled_ = enabled;

  if (enabled) {
    switch_access_loader_->Load(profile_, base::Closure() /* done_cb */);
    switch_access_event_handler_.reset(
        new chromeos::SwitchAccessEventHandler());
  } else {
    switch_access_loader_->Unload();
    switch_access_event_handler_.reset(nullptr);
  }
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
      BrowserThread::IO, FROM_HERE,
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
  std::string preload_engines_str =
      pref_service->GetString(prefs::kLanguagePreloadEngines);
  std::vector<base::StringPiece> preload_engines = base::SplitStringPiece(
      preload_engines_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<base::StringPiece>::iterator it =
      std::find(preload_engines.begin(), preload_engines.end(),
                extension_ime_util::kBrailleImeEngineId);
  bool is_enabled = (it != preload_engines.end());
  bool should_be_enabled =
      (spoken_feedback_enabled_ && braille_display_connected_);
  if (is_enabled == should_be_enabled)
    return;
  if (should_be_enabled)
    preload_engines.push_back(extension_ime_util::kBrailleImeEngineId);
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
  if (GetAshConfig() != ash::Config::MASH) {
    ash::Shell::Get()->sticky_keys_controller()->SetModifiersEnabled(
        manager->IsISOLevel5ShiftUsedByCurrentInputMethod(),
        manager->IsAltGrUsedByCurrentInputMethod());
  }
  const chromeos::input_method::InputMethodDescriptor descriptor =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  braille_ime_current_ =
      (descriptor.id() == extension_ime_util::kBrailleImeEngineId);
}

void AccessibilityManager::OnSessionStateChanged() {
  if (!chromevox_panel_)
    return;
  if (chromevox_panel_->for_blocked_user_session() ==
      session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return;
  }

  // If user session got blocked or unblocked, reload ChromeVox panel, as
  // functionality available to the panel differs based on whether the user
  // session is active (and unlocked) or not.
  ReloadChromeVoxPanel();
}

void AccessibilityManager::SetProfile(Profile* profile) {
  // Do nothing if this is called for the current profile. This can happen. For
  // example, ChromeSessionManager fires both
  // NOTIFICATION_LOGIN_USER_PROFILE_PREPARED and NOTIFICATION_SESSION_STARTED,
  // and we are observing both events.
  if (profile_ == profile)
    return;

  pref_change_registrar_.reset();
  local_state_pref_change_registrar_.reset();

  if (profile) {
    // TODO(yoshiki): Move following code to PrefHandler.
    pref_change_registrar_.reset(new PrefChangeRegistrar);
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        ash::prefs::kShouldAlwaysShowAccessibilityMenu,
        base::Bind(&AccessibilityManager::UpdateAlwaysShowMenuFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityLargeCursorEnabled,
        base::Bind(&AccessibilityManager::OnLargeCursorChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityLargeCursorDipSize,
        base::Bind(&AccessibilityManager::OnLargeCursorChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityStickyKeysEnabled,
        base::Bind(&AccessibilityManager::UpdateStickyKeysFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilitySpokenFeedbackEnabled,
        base::Bind(&AccessibilityManager::UpdateSpokenFeedbackFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityHighContrastEnabled,
        base::Bind(&AccessibilityManager::OnHighContrastChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityAutoclickEnabled,
        base::Bind(&AccessibilityManager::UpdateAutoclickFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityAutoclickDelayMs,
        base::Bind(&AccessibilityManager::UpdateAutoclickDelayFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityVirtualKeyboardEnabled,
        base::Bind(&AccessibilityManager::UpdateVirtualKeyboardFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityMonoAudioEnabled,
        base::Bind(&AccessibilityManager::UpdateMonoAudioFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityCaretHighlightEnabled,
        base::Bind(&AccessibilityManager::UpdateCaretHighlightFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityCursorHighlightEnabled,
        base::Bind(&AccessibilityManager::UpdateCursorHighlightFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityFocusHighlightEnabled,
        base::Bind(&AccessibilityManager::UpdateFocusHighlightFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kTapDraggingEnabled,
        base::Bind(&AccessibilityManager::UpdateTapDraggingFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilitySelectToSpeakEnabled,
        base::Bind(&AccessibilityManager::UpdateSelectToSpeakFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilitySwitchAccessEnabled,
        base::Bind(&AccessibilityManager::UpdateSwitchAccessFromPref,
                   base::Unretained(this)));

    local_state_pref_change_registrar_.reset(new PrefChangeRegistrar);
    local_state_pref_change_registrar_->Init(g_browser_process->local_state());
    local_state_pref_change_registrar_->Add(
        prefs::kApplicationLocale,
        base::Bind(&AccessibilityManager::OnLocaleChanged,
                   base::Unretained(this)));

    content::BrowserAccessibilityState::GetInstance()->AddHistogramCallback(
        base::Bind(&AccessibilityManager::UpdateChromeOSAccessibilityHistograms,
                   base::Unretained(this)));

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    if (!extension_registry_observer_.IsObserving(registry))
      extension_registry_observer_.Add(registry);
  }

  large_cursor_pref_handler_.HandleProfileChanged(profile_, profile);
  spoken_feedback_pref_handler_.HandleProfileChanged(profile_, profile);
  high_contrast_pref_handler_.HandleProfileChanged(profile_, profile);
  sticky_keys_pref_handler_.HandleProfileChanged(profile_, profile);
  autoclick_pref_handler_.HandleProfileChanged(profile_, profile);
  autoclick_delay_pref_handler_.HandleProfileChanged(profile_, profile);
  virtual_keyboard_pref_handler_.HandleProfileChanged(profile_, profile);
  mono_audio_pref_handler_.HandleProfileChanged(profile_, profile);
  caret_highlight_pref_handler_.HandleProfileChanged(profile_, profile);
  cursor_highlight_pref_handler_.HandleProfileChanged(profile_, profile);
  focus_highlight_pref_handler_.HandleProfileChanged(profile_, profile);
  tap_dragging_pref_handler_.HandleProfileChanged(profile_, profile);
  select_to_speak_pref_handler_.HandleProfileChanged(profile_, profile);
  switch_access_pref_handler_.HandleProfileChanged(profile_, profile);

  bool had_profile = (profile_ != NULL);
  profile_ = profile;

  if (!had_profile && profile)
    CheckBrailleState();
  else
    UpdateBrailleImeState();
  UpdateAlwaysShowMenuFromPref();
  UpdateStickyKeysFromPref();
  UpdateSpokenFeedbackFromPref();
  UpdateAutoclickFromPref();
  UpdateAutoclickDelayFromPref();
  UpdateVirtualKeyboardFromPref();
  UpdateMonoAudioFromPref();
  UpdateCaretHighlightFromPref();
  UpdateCursorHighlightFromPref();
  UpdateFocusHighlightFromPref();
  UpdateTapDraggingFromPref();
  UpdateSelectToSpeakFromPref();
  UpdateSwitchAccessFromPref();

  // Update the panel height in the shelf layout manager when the profile
  // changes, since the shelf layout manager doesn't exist in the login profile.
  if (chromevox_panel_)
    chromevox_panel_->UpdatePanelHeight();
}

void AccessibilityManager::ActiveUserChanged(
    const user_manager::User* active_user) {
  if (active_user && active_user->is_profile_created())
    SetProfile(ProfileManager::GetActiveUserProfile());
}

void AccessibilityManager::OnFullscreenStateChanged(bool is_fullscreen,
                                                    aura::Window* root_window) {
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

base::TimeDelta AccessibilityManager::PlayShutdownSound() {
  if (!PlayEarcon(SOUND_SHUTDOWN, PlaySoundOption::SPOKEN_FEEDBACK_ENABLED))
    return base::TimeDelta();
  return media::SoundsManager::Get()->GetDuration(SOUND_SHUTDOWN);
}

std::unique_ptr<AccessibilityStatusSubscription>
AccessibilityManager::RegisterCallback(const AccessibilityStatusCallback& cb) {
  return callback_list_.Add(cb);
}

void AccessibilityManager::NotifyAccessibilityStatusChanged(
    AccessibilityStatusEventDetails& details) {
  callback_list_.Notify(details);

  // TODO(crbug.com/594887): Fix for mash by moving pref into ash.
  if (GetAshConfig() == ash::Config::MASH)
    return;

  // Update system tray menu visibility. Prefs tracked inside ash handle their
  // own updates to avoid race conditions (pref updates are asynchronous between
  // chrome and ash).
  if (details.notification_type != ACCESSIBILITY_MANAGER_SHUTDOWN &&
      details.notification_type != ACCESSIBILITY_TOGGLE_LARGE_CURSOR) {
    ash::Shell::Get()->system_tray_notifier()->NotifyAccessibilityStatusChanged(
        details.notify);
  }
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
    UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosScreenMagnifier",
                          MagnificationManager::Get()->IsMagnifierEnabled());
  }
  if (profile_) {
    const PrefService* const prefs = profile_->GetPrefs();

    bool large_cursor_enabled =
        prefs->GetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled);
    UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosLargeCursor",
                          large_cursor_enabled);
    if (large_cursor_enabled) {
      UMA_HISTOGRAM_COUNTS_100(
          "Accessibility.CrosLargeCursorSize",
          prefs->GetInteger(ash::prefs::kAccessibilityLargeCursorDipSize));
    }

    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.CrosAlwaysShowA11yMenu",
        prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu));

    bool autoclick_enabled =
        prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled);
    UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosAutoclick", autoclick_enabled);
    if (autoclick_enabled) {
      // We only want to log the autoclick delay if the user has actually
      // enabled autoclick.
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Accessibility.CrosAutoclickDelay",
          base::TimeDelta::FromMilliseconds(
              prefs->GetInteger(ash::prefs::kAccessibilityAutoclickDelayMs)),
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMilliseconds(3000), 50);
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
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED:
      // Update |profile_| when login user profile is prepared.
      // NOTIFICATION_SESSION_STARTED is not fired from UserSessionManager, but
      // profile may be changed by UserSessionManager in OOBE flow.
      SetProfile(ProfileManager::GetActiveUserProfile());
      break;
    case chrome::NOTIFICATION_SESSION_STARTED:
      // Update |profile_| when entering a session.
      SetProfile(ProfileManager::GetActiveUserProfile());

      // Add a session state observer to be able to monitor session changes.
      if (!session_state_observer_.get())
        session_state_observer_.reset(
            new user_manager::ScopedUserSessionStateObserver(this));
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Update |profile_| when exiting a session or shutting down.
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile_ == profile)
        SetProfile(NULL);
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
        ->ChangeInputMethod(extension_ime_util::kBrailleImeEngineId,
                            false /* show_message */);
  }
}

void AccessibilityManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
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

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  CHECK(event_router);

  std::unique_ptr<base::ListValue> event_args(new base::ListValue());
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_INTRODUCE_CHROME_VOX,
      extensions::api::accessibility_private::OnIntroduceChromeVox::kEventName,
      std::move(event_args)));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kChromeVoxExtensionId, std::move(event));

  if (!chromevox_panel_) {
    chromevox_panel_ = new ChromeVoxPanel(
        profile_,
        session_manager::SessionManager::Get()->IsUserSessionBlocked());
    chromevox_panel_widget_observer_.reset(
        new ChromeVoxPanelWidgetObserver(chromevox_panel_->GetWidget(), this));
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableAudioFocus)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableAudioFocus);
  }
}

void AccessibilityManager::PostUnloadChromeVox() {
  // Do any teardown work needed immediately after ChromeVox actually unloads.
  PlayEarcon(SOUND_SPOKEN_FEEDBACK_DISABLED, PlaySoundOption::ALWAYS);
  // Clear the accessibility focus ring.
  ash::AccessibilityFocusRingController::GetInstance()->SetFocusRing(
      std::vector<gfx::Rect>(),
      ash::AccessibilityFocusRingController::PERSIST_FOCUS_RING);

  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }

  // In case the user darkened the screen, undarken it now.
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetBacklightsForcedOff(false);
}

void AccessibilityManager::PostSwitchChromeVoxProfile() {
  ReloadChromeVoxPanel();
}

void AccessibilityManager::ReloadChromeVoxPanel() {
  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }
  chromevox_panel_ = new ChromeVoxPanel(
      profile_, session_manager::SessionManager::Get()->IsUserSessionBlocked());
  chromevox_panel_widget_observer_.reset(
      new ChromeVoxPanelWidgetObserver(chromevox_panel_->GetWidget(), this));
}

void AccessibilityManager::OnChromeVoxPanelClosing() {
  chromevox_panel_->ResetPanelHeight();
  chromevox_panel_widget_observer_.reset();
  chromevox_panel_ = nullptr;
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

void AccessibilityManager::SetSwitchAccessKeys(const std::set<int>& key_codes) {
  if (switch_access_enabled_)
    switch_access_event_handler_->SetKeysToCapture(key_codes);
}

}  // namespace chromeos

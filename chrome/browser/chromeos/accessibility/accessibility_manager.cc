// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/interfaces/accessibility_focus_ring_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_extension_loader.h"
#include "chrome/browser/chromeos/accessibility/dictation_chromeos.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler.h"
#include "chrome/browser/chromeos/accessibility/switch_access_event_handler.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/host_id.h"
#include "mash/public/mojom/launchable.mojom.h"
#include "media/audio/sounds/sounds_manager.h"
#include "media/base/media_switches.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

using content::BrowserThread;
using extensions::api::braille_display_private::BrailleController;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::StubBrailleController;

namespace chromeos {

namespace {

// When this flag is set, system sounds will not be played.
constexpr char kAshDisableSystemSounds[] = "ash-disable-system-sounds";

static chromeos::AccessibilityManager* g_accessibility_manager = nullptr;

static BrailleController* g_braille_controller_for_test = nullptr;

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

  ~ChromeVoxPanelWidgetObserver() override = default;

  void OnWidgetClosing(views::Widget* widget) override {
    CHECK_EQ(widget_, widget);
    widget->RemoveObserver(this);
    manager_->OnChromeVoxPanelClosing();
    // |this| is deleted.
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    CHECK_EQ(widget_, widget);
    widget->RemoveObserver(this);
    manager_->OnChromeVoxPanelDestroying();
    // |this| is deleted.
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
    bool enabled)
    : notification_type(notification_type), enabled(enabled) {}

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

// static
void AccessibilityManager::ShowAccessibilityHelp(Browser* browser) {
  ShowSingletonTab(browser, GURL(chrome::kChromeAccessibilityHelpURL));
}

AccessibilityManager::AccessibilityManager()
    : profile_(NULL),
      spoken_feedback_enabled_(false),
      select_to_speak_enabled_(false),
      switch_access_enabled_(false),
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
  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
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
  manager->Initialize(SOUND_DICTATION_END,
                      bundle.GetRawDataResource(IDR_SOUND_DICTATION_END_WAV));
  manager->Initialize(SOUND_DICTATION_START,
                      bundle.GetRawDataResource(IDR_SOUND_DICTATION_START_WAV));

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
      base::Bind(&AccessibilityManager::PostUnloadSelectToSpeak,
                 weak_ptr_factory_.GetWeakPtr())));
  switch_access_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kSwitchAccessExtensionId,
      resources_path.Append(extension_misc::kSwitchAccessExtensionPath),
      base::Closure()));

  // Connect to ash's AccessibilityController interface.
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &accessibility_controller_);

  // Connect to ash's AccessibilityFocusRingController interface.
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName,
                      &accessibility_focus_ring_controller_);
}

AccessibilityManager::~AccessibilityManager() {
  CHECK(this == g_accessibility_manager);
  AccessibilityStatusEventDetails details(ACCESSIBILITY_MANAGER_SHUTDOWN,
                                          false);
  NotifyAccessibilityStatusChanged(details);
  input_method::InputMethodManager::Get()->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);

  if (chromevox_panel_) {
    chromevox_panel_->CloseNow();
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
        prefs->GetBoolean(ash::prefs::kAccessibilitySelectToSpeakEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityHighContrastEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled) ||
        prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityVirtualKeyboardEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityMonoAudioEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityCaretHighlightEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityCursorHighlightEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled) ||
        prefs->GetBoolean(ash::prefs::kDockedMagnifierEnabled)) {
      return true;
    }
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
  ash::Shell::Get()
      ->accessibility_controller()
      ->NotifyAccessibilityStatusChanged();
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
                                          IsLargeCursorEnabled());
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
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityStickyKeysEnabled);
}

void AccessibilityManager::OnStickyKeysChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_STICKY_KEYS,
                                          IsStickyKeysEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::EnableSpokenFeedback(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::OnSpokenFeedbackChanged() {
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
                                          enabled);
  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    chromevox_loader_->Load(profile_,
                            base::Bind(&AccessibilityManager::PostLoadChromeVox,
                                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    chromevox_loader_->Unload();
  }
  UpdateBrailleImeState();
}

bool AccessibilityManager::IsSpokenFeedbackEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilitySpokenFeedbackEnabled);
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
      ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE, IsHighContrastEnabled());
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
  EnableSpokenFeedback(false);
  EnableSpokenFeedback(true);
}

void AccessibilityManager::OnViewFocusedInArc(
    const gfx::Rect& bounds_in_screen) {
  accessibility_controller_->SetFocusHighlightRect(bounds_in_screen);
}

bool AccessibilityManager::PlayEarcon(int sound_key, PlaySoundOption option) {
  DCHECK(sound_key < chromeos::SOUND_COUNT);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kAshDisableSystemSounds))
    return false;
  if (option == PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED &&
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

  auto event_args = std::make_unique<base::ListValue>();
  auto event = std::make_unique<extensions::Event>(
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

  auto event_args = std::make_unique<base::ListValue>();
  auto event = std::make_unique<extensions::Event>(
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

void AccessibilityManager::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture) {
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
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityAutoclickEnabled);
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
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityVirtualKeyboardEnabled);
}

void AccessibilityManager::OnVirtualKeyboardChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD,
                                          IsVirtualKeyboardEnabled());
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
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityMonoAudioEnabled);
}

void AccessibilityManager::OnMonoAudioChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_MONO_AUDIO,
                                          IsMonoAudioEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::SetDarkenScreen(bool darken) {
  accessibility_controller_->SetDarkenScreen(darken);
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
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityCaretHighlightEnabled);
}

void AccessibilityManager::OnCaretHighlightChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_CARET_HIGHLIGHT,
                                          IsCaretHighlightEnabled());
  NotifyAccessibilityStatusChanged(details);
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
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityCursorHighlightEnabled);
}

void AccessibilityManager::OnCursorHighlightChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_CURSOR_HIGHLIGHT,
                                          IsCursorHighlightEnabled());
  NotifyAccessibilityStatusChanged(details);
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
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityFocusHighlightEnabled);
}

void AccessibilityManager::OnFocusHighlightChanged() {
  bool enabled = IsFocusHighlightEnabled();

  // Focus highlighting can't be on when spoken feedback is on, because
  // ChromeVox does its own focus highlighting.
  if (IsSpokenFeedbackEnabled())
    enabled = false;
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_FOCUS_HIGHLIGHT,
                                          enabled);
  NotifyAccessibilityStatusChanged(details);
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

void AccessibilityManager::OnSelectToSpeakChanged() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySelectToSpeakEnabled);

  if (select_to_speak_enabled_ == enabled)
    return;
  select_to_speak_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_SELECT_TO_SPEAK,
                                          enabled);
  NotifyAccessibilityStatusChanged(details);

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
      (IsSpokenFeedbackEnabled() && braille_display_connected_);
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
  // Don't reload ChromeVox during shutdown. http://crrev.com/c/838180
  if (app_terminating_)
    return;

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

  // Clear all dictation state on profile change.
  dictation_.reset();

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
        base::Bind(&AccessibilityManager::OnStickyKeysChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilitySpokenFeedbackEnabled,
        base::Bind(&AccessibilityManager::OnSpokenFeedbackChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityHighContrastEnabled,
        base::Bind(&AccessibilityManager::OnHighContrastChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityVirtualKeyboardEnabled,
        base::Bind(&AccessibilityManager::OnVirtualKeyboardChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityMonoAudioEnabled,
        base::Bind(&AccessibilityManager::OnMonoAudioChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityCaretHighlightEnabled,
        base::Bind(&AccessibilityManager::OnCaretHighlightChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityCursorHighlightEnabled,
        base::Bind(&AccessibilityManager::OnCursorHighlightChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityFocusHighlightEnabled,
        base::Bind(&AccessibilityManager::OnFocusHighlightChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilitySelectToSpeakEnabled,
        base::Bind(&AccessibilityManager::OnSelectToSpeakChanged,
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

  bool had_profile = (profile_ != NULL);
  profile_ = profile;

  if (!had_profile && profile)
    CheckBrailleState();
  else
    UpdateBrailleImeState();
  UpdateAlwaysShowMenuFromPref();
  UpdateSwitchAccessFromPref();

  // TODO(warx): reconcile to ash once the prefs registration above is moved to
  // ash.
  OnSpokenFeedbackChanged();
  OnSelectToSpeakChanged();
}

void AccessibilityManager::ActiveUserChanged(
    const user_manager::User* active_user) {
  if (active_user && active_user->is_profile_created())
    SetProfile(ProfileManager::GetActiveUserProfile());
}

base::TimeDelta AccessibilityManager::PlayShutdownSound() {
  if (!PlayEarcon(SOUND_SHUTDOWN,
                  PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED)) {
    return base::TimeDelta();
  }
  return media::SoundsManager::Get()->GetDuration(SOUND_SHUTDOWN);
}

std::unique_ptr<AccessibilityStatusSubscription>
AccessibilityManager::RegisterCallback(const AccessibilityStatusCallback& cb) {
  return callback_list_.Add(cb);
}

void AccessibilityManager::NotifyAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  callback_list_.Notify(details);

  // TODO(crbug.com/594887): Fix for mash by moving pref into ash.
  if (GetAshConfig() == ash::Config::MASH)
    return;

  // Update system tray menu visibility. Prefs tracked inside ash handle their
  // own updates to avoid race conditions (pref updates are asynchronous between
  // chrome and ash).
  if (details.notification_type == ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER) {
    ash::Shell::Get()
        ->accessibility_controller()
        ->NotifyAccessibilityStatusChanged();
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
    case chrome::NOTIFICATION_APP_TERMINATING: {
      app_terminating_ = true;
      break;
    }
    case content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE: {
      content::FocusedNodeDetails* node_details =
          content::Details<content::FocusedNodeDetails>(details).ptr();
      accessibility_controller_->SetFocusHighlightRect(
          node_details->node_bounds_in_screen);
      break;
    }
  }
}

void AccessibilityManager::OnBrailleDisplayStateChanged(
    const DisplayState& display_state) {
  braille_display_connected_ = display_state.available;
  accessibility_controller_->BrailleDisplayStateChanged(
      braille_display_connected_);
  UpdateBrailleImeState();
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
  // In browser_tests loading the ChromeVox extension can race with shutdown.
  // http://crbug.com/801700
  if (app_terminating_)
    return;

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
  SetFocusRing(std::vector<gfx::Rect>(),
               ash::mojom::FocusRingBehavior::PERSIST_FOCUS_RING);

  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }

  // In case the user darkened the screen, undarken it now.
  SetDarkenScreen(false);

  // Stop speech.
  TtsController::GetInstance()->Stop();
}

void AccessibilityManager::PostSwitchChromeVoxProfile() {
  ReloadChromeVoxPanel();
}

void AccessibilityManager::ReloadChromeVoxPanel() {
  if (chromevox_panel_) {
    chromevox_panel_->CloseNow();
    chromevox_panel_ = nullptr;
  }
  chromevox_panel_ = new ChromeVoxPanel(
      profile_, session_manager::SessionManager::Get()->IsUserSessionBlocked());
  chromevox_panel_widget_observer_.reset(
      new ChromeVoxPanelWidgetObserver(chromevox_panel_->GetWidget(), this));
}

void AccessibilityManager::OnChromeVoxPanelClosing() {
  chromevox_panel_widget_observer_.reset();
  chromevox_panel_ = nullptr;
}

void AccessibilityManager::OnChromeVoxPanelDestroying() {
  chromevox_panel_widget_observer_.reset(nullptr);
  chromevox_panel_ = nullptr;
}

void AccessibilityManager::PostUnloadSelectToSpeak() {
  // Do any teardown work needed immediately after Select-to-Speak actually
  // unloads.

  // Clear the accessibility focus ring and highlight.
  HideFocusRing();
  HideHighlights();

  // Stop speech.
  TtsController::GetInstance()->Stop();
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

void AccessibilityManager::ToggleDictation() {
  if (!profile_)
    return;

  if (!dictation_.get())
    dictation_ = std::make_unique<DictationChromeos>(profile_);

  dictation_->OnToggleDictation();
}

void AccessibilityManager::SetFocusRingColor(SkColor color) {
  accessibility_focus_ring_controller_->SetFocusRingColor(color);
}

void AccessibilityManager::ResetFocusRingColor() {
  accessibility_focus_ring_controller_->ResetFocusRingColor();
}

void AccessibilityManager::SetFocusRing(
    const std::vector<gfx::Rect>& rects_in_screen,
    ash::mojom::FocusRingBehavior focus_ring_behavior) {
  accessibility_focus_ring_controller_->SetFocusRing(rects_in_screen,
                                                     focus_ring_behavior);
  if (focus_ring_observer_for_test_)
    focus_ring_observer_for_test_.Run();
}

void AccessibilityManager::HideFocusRing() {
  accessibility_focus_ring_controller_->HideFocusRing();
  if (focus_ring_observer_for_test_)
    focus_ring_observer_for_test_.Run();
}

void AccessibilityManager::SetHighlights(
    const std::vector<gfx::Rect>& rects_in_screen,
    SkColor color) {
  accessibility_focus_ring_controller_->SetHighlights(rects_in_screen, color);
}

void AccessibilityManager::HideHighlights() {
  accessibility_focus_ring_controller_->HideHighlights();
}

void AccessibilityManager::SetProfileForTest(Profile* profile) {
  SetProfile(profile);
}

// static
void AccessibilityManager::SetBrailleControllerForTest(
    BrailleController* controller) {
  g_braille_controller_for_test = controller;
}

void AccessibilityManager::FlushForTesting() {
  accessibility_controller_.FlushForTesting();
}

void AccessibilityManager::SetFocusRingObserverForTest(
    base::RepeatingCallback<void()> observer) {
  focus_ring_observer_for_test_ = observer;
}

}  // namespace chromeos

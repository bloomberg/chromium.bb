// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include "ash/audio/sounds.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/login/login_state.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/file_reader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_resource.h"
#include "media/audio/sounds/sounds_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

using content::BrowserThread;
using content::RenderViewHost;
using extensions::api::braille_display_private::BrailleController;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;

namespace chromeos {

namespace {

static chromeos::AccessibilityManager* g_accessibility_manager = NULL;

static BrailleController* g_braille_controller_for_test = NULL;

BrailleController* GetBrailleController() {
  return g_braille_controller_for_test
      ? g_braille_controller_for_test
      : BrailleController::GetInstance();
}

base::FilePath GetChromeVoxPath() {
  base::FilePath path;
  if (!PathService::Get(chrome::DIR_RESOURCES, &path))
    NOTREACHED();
  path = path.Append(extension_misc::kChromeVoxExtensionPath);
  return path;
}

// Helper class that directly loads an extension's content scripts into
// all of the frames corresponding to a given RenderViewHost.
class ContentScriptLoader {
 public:
  // Initialize the ContentScriptLoader with the ID of the extension
  // and the RenderViewHost where the scripts should be loaded.
  ContentScriptLoader(const std::string& extension_id,
                      int render_process_id,
                      int render_view_id)
      : extension_id_(extension_id),
        render_process_id_(render_process_id),
        render_view_id_(render_view_id) {}

  // Call this once with the ExtensionResource corresponding to each
  // content script to be loaded.
  void AppendScript(extensions::ExtensionResource resource) {
    resources_.push(resource);
  }

  // Finally, call this method once to fetch all of the resources and
  // load them. This method will delete this object when done.
  void Run() {
    if (resources_.empty()) {
      delete this;
      return;
    }

    extensions::ExtensionResource resource = resources_.front();
    resources_.pop();
    scoped_refptr<FileReader> reader(new FileReader(resource, base::Bind(
        &ContentScriptLoader::OnFileLoaded, base::Unretained(this))));
    reader->Start();
  }

 private:
  void OnFileLoaded(bool success, const std::string& data) {
    if (success) {
      ExtensionMsg_ExecuteCode_Params params;
      params.request_id = 0;
      params.extension_id = extension_id_;
      params.is_javascript = true;
      params.code = data;
      params.run_at = extensions::UserScript::DOCUMENT_IDLE;
      params.all_frames = true;
      params.match_about_blank = false;
      params.in_main_world = false;

      RenderViewHost* render_view_host =
          RenderViewHost::FromID(render_process_id_, render_view_id_);
      if (render_view_host) {
        render_view_host->Send(new ExtensionMsg_ExecuteCode(
            render_view_host->GetRoutingID(), params));
      }
    }
    Run();
  }

  std::string extension_id_;
  int render_process_id_;
  int render_view_id_;
  std::queue<extensions::ExtensionResource> resources_;
};

void InjectChromeVoxContentScript(
    ExtensionService* extension_service,
    int render_process_id,
    int render_view_id,
    const base::Closure& done_cb);

void LoadChromeVoxExtension(
    Profile* profile,
    RenderViewHost* render_view_host,
    base::Closure done_cb) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (render_view_host) {
    // Wrap the passed in callback to inject the content script.
    done_cb = base::Bind(
        &InjectChromeVoxContentScript,
        extension_service,
        render_view_host->GetProcess()->GetID(),
        render_view_host->GetRoutingID(),
        done_cb);
  }
  extension_service->component_loader()->AddChromeVoxExtension(done_cb);
}

void InjectChromeVoxContentScript(
    ExtensionService* extension_service,
    int render_process_id,
    int render_view_id,
    const base::Closure& done_cb) {
  // Make sure to always run |done_cb|.  ChromeVox was loaded even if we end up
  // not injecting into this particular render view.
  base::ScopedClosureRunner done_runner(done_cb);
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return;
  const extensions::Extension* extension =
      extension_service->extensions()->GetByID(
          extension_misc::kChromeVoxExtensionId);

  // Set a flag to tell ChromeVox that it's just been enabled,
  // so that it won't interrupt our speech feedback enabled message.
  ExtensionMsg_ExecuteCode_Params params;
  params.request_id = 0;
  params.extension_id = extension->id();
  params.is_javascript = true;
  params.code = "window.INJECTED_AFTER_LOAD = true;";
  params.run_at = extensions::UserScript::DOCUMENT_IDLE;
  params.all_frames = true;
  params.match_about_blank = false;
  params.in_main_world = false;
  render_view_host->Send(new ExtensionMsg_ExecuteCode(
      render_view_host->GetRoutingID(), params));

  // Inject ChromeVox' content scripts.
  ContentScriptLoader* loader = new ContentScriptLoader(
      extension->id(), render_view_host->GetProcess()->GetID(),
      render_view_host->GetRoutingID());

  const extensions::UserScriptList& content_scripts =
      extensions::ContentScriptsInfo::GetContentScripts(extension);
  for (size_t i = 0; i < content_scripts.size(); i++) {
    const extensions::UserScript& script = content_scripts[i];
    for (size_t j = 0; j < script.js_scripts().size(); ++j) {
      const extensions::UserScript::File& file = script.js_scripts()[j];
      extensions::ExtensionResource resource = extension->GetResource(
          file.relative_path());
      loader->AppendScript(resource);
    }
  }
  loader->Run();  // It cleans itself up when done.
}

void UnloadChromeVoxExtension(Profile* profile) {
  base::FilePath path = GetChromeVoxPath();
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->component_loader()->Remove(path);
}

}  // namespace

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
      chrome_vox_loaded_on_lock_screen_(false),
      chrome_vox_loaded_on_user_screen_(false),
      large_cursor_pref_handler_(prefs::kAccessibilityLargeCursorEnabled),
      spoken_feedback_pref_handler_(prefs::kAccessibilitySpokenFeedbackEnabled),
      high_contrast_pref_handler_(prefs::kAccessibilityHighContrastEnabled),
      autoclick_pref_handler_(prefs::kAccessibilityAutoclickEnabled),
      autoclick_delay_pref_handler_(prefs::kAccessibilityAutoclickDelayMs),
      virtual_keyboard_pref_handler_(
          prefs::kAccessibilityVirtualKeyboardEnabled),
      large_cursor_enabled_(false),
      sticky_keys_enabled_(false),
      spoken_feedback_enabled_(false),
      high_contrast_enabled_(false),
      autoclick_enabled_(false),
      autoclick_delay_ms_(ash::AutoclickController::kDefaultAutoclickDelayMs),
      virtual_keyboard_enabled_(false),
      spoken_feedback_notification_(ash::A11Y_NOTIFICATION_NONE),
      weak_ptr_factory_(this),
      should_speak_chrome_vox_announcements_on_user_screen_(true),
      system_sounds_enabled_(false),
      braille_display_connected_(false),
      scoped_braille_observer_(this),
      braille_ime_current_(false) {
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
}

AccessibilityManager::~AccessibilityManager() {
  CHECK(this == g_accessibility_manager);
  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_MANAGER_SHUTDOWN,
      false,
      ash::A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged(details);
  input_method::InputMethodManager::Get()->RemoveObserver(this);
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
        pref_service->GetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled))
      return true;
  }
  return false;
}

bool AccessibilityManager::ShouldEnableCursorCompositing() {
#if defined(OS_CHROMEOS)
  if (!profile_)
    return false;
  PrefService* pref_service = profile_->GetPrefs();
  // Enable cursor compositing when one or more of the listed accessibility
  // features are turned on.
  if (pref_service->GetBoolean(prefs::kAccessibilityLargeCursorEnabled) ||
      pref_service->GetBoolean(prefs::kAccessibilityHighContrastEnabled) ||
      pref_service->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled))
    return true;
#endif
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

  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_TOGGLE_LARGE_CURSOR,
      enabled,
      ash::A11Y_NOTIFICATION_NONE);

  NotifyAccessibilityStatusChanged(details);

#if defined(USE_ASH)
  // Large cursor is implemented only in ash.
  ash::Shell::GetInstance()->cursor_manager()->SetCursorSet(
      enabled ? ui::CURSOR_SET_LARGE : ui::CURSOR_SET_NORMAL);
#endif

#if defined(OS_CHROMEOS)
  ash::Shell::GetInstance()->SetCursorCompositingEnabled(
      ShouldEnableCursorCompositing());
#endif
}

bool AccessibilityManager::IsIncognitoAllowed() {
  // Supervised users can't create incognito-mode windows.
  return !(user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser());
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
#if defined(USE_ASH)
  // Sticky keys is implemented only in ash.
  ash::Shell::GetInstance()->sticky_keys_controller()->Enable(enabled);
#endif
}

void AccessibilityManager::EnableSpokenFeedback(
    bool enabled,
    ash::AccessibilityNotificationVisibility notify) {
  if (!profile_)
    return;

  ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
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

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  ExtensionAccessibilityEventRouter::GetInstance()->
      SetAccessibilityEnabled(enabled);

  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
      enabled,
      spoken_feedback_notification_);

  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    LoadChromeVox();
  } else {
    UnloadChromeVox();
  }
  UpdateBrailleImeState();
}

void AccessibilityManager::LoadChromeVox() {
  base::Closure done_cb = base::Bind(&AccessibilityManager::PostLoadChromeVox,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     profile_);
  ScreenLocker* screen_locker = ScreenLocker::default_screen_locker();
  if (screen_locker && screen_locker->locked()) {
    // If on the lock screen, loads ChromeVox only to the lock screen as for
    // now. On unlock, it will be loaded to the user screen.
    // (see. AccessibilityManager::Observe())
    LoadChromeVoxToLockScreen(done_cb);
  } else {
    LoadChromeVoxToUserScreen(done_cb);
  }
}

void AccessibilityManager::LoadChromeVoxToUserScreen(
    const base::Closure& done_cb) {
  if (chrome_vox_loaded_on_user_screen_)
    return;

  // Determine whether an OOBE screen is currently being shown. If so,
  // ChromeVox will be injected directly into that screen.
  content::WebUI* login_web_ui = NULL;

  if (ProfileHelper::IsSigninProfile(profile_)) {
    LoginDisplayHost* login_display_host = LoginDisplayHostImpl::default_host();
    if (login_display_host) {
      WebUILoginView* web_ui_login_view =
          login_display_host->GetWebUILoginView();
      if (web_ui_login_view)
        login_web_ui = web_ui_login_view->GetWebUI();
    }

    // Lock screen uses the signin progile.
    chrome_vox_loaded_on_lock_screen_ = true;
  }

  chrome_vox_loaded_on_user_screen_ = true;
  LoadChromeVoxExtension(
      profile_, login_web_ui ?
      login_web_ui->GetWebContents()->GetRenderViewHost() : NULL,
      done_cb);
}

void AccessibilityManager::LoadChromeVoxToLockScreen(
    const base::Closure& done_cb) {
  if (chrome_vox_loaded_on_lock_screen_)
    return;

  ScreenLocker* screen_locker = ScreenLocker::default_screen_locker();
  if (screen_locker && screen_locker->locked()) {
    content::WebUI* lock_web_ui = screen_locker->GetAssociatedWebUI();
    if (lock_web_ui) {
      Profile* profile = Profile::FromWebUI(lock_web_ui);
      chrome_vox_loaded_on_lock_screen_ = true;
      LoadChromeVoxExtension(
          profile,
          lock_web_ui->GetWebContents()->GetRenderViewHost(),
          done_cb);
    }
  }
}

void AccessibilityManager::UnloadChromeVox() {
  if (chrome_vox_loaded_on_lock_screen_)
    UnloadChromeVoxFromLockScreen();

  if (chrome_vox_loaded_on_user_screen_) {
    UnloadChromeVoxExtension(profile_);
    chrome_vox_loaded_on_user_screen_ = false;
  }

  PostUnloadChromeVox(profile_);
}

void AccessibilityManager::UnloadChromeVoxFromLockScreen() {
  // Lock screen uses the signin progile.
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  UnloadChromeVoxExtension(signin_profile);
  chrome_vox_loaded_on_lock_screen_ = false;
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
      ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE,
      enabled,
      ash::A11Y_NOTIFICATION_NONE);

  NotifyAccessibilityStatusChanged(details);

#if defined(USE_ASH)
  ash::Shell::GetInstance()->high_contrast_controller()->SetEnabled(enabled);
#endif

#if defined(OS_CHROMEOS)
  ash::Shell::GetInstance()->SetCursorCompositingEnabled(
      ShouldEnableCursorCompositing());
#endif
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

void AccessibilityManager::PlayEarcon(int sound_key) {
  DCHECK(sound_key < chromeos::SOUND_COUNT);
  ash::PlaySystemSoundIfSpokenFeedback(sound_key);
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
  bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityAutoclickEnabled);

  if (autoclick_enabled_ == enabled)
    return;
  autoclick_enabled_ = enabled;

#if defined(USE_ASH)
  ash::Shell::GetInstance()->autoclick_controller()->SetEnabled(enabled);
#endif
}

void AccessibilityManager::SetAutoclickDelay(int delay_ms) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInteger(prefs::kAccessibilityAutoclickDelayMs, delay_ms);
  pref_service->CommitPendingWrite();
}

int AccessibilityManager::GetAutoclickDelay() const {
  return autoclick_delay_ms_;
}

void AccessibilityManager::UpdateAutoclickDelayFromPref() {
  int autoclick_delay_ms =
      profile_->GetPrefs()->GetInteger(prefs::kAccessibilityAutoclickDelayMs);

  if (autoclick_delay_ms == autoclick_delay_ms_)
    return;
  autoclick_delay_ms_ = autoclick_delay_ms;

#if defined(USE_ASH)
  ash::Shell::GetInstance()->autoclick_controller()->SetAutoclickDelay(
      autoclick_delay_ms_);
#endif
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

  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD,
      enabled,
      ash::A11Y_NOTIFICATION_NONE);

  NotifyAccessibilityStatusChanged(details);

#if defined(USE_ASH)
  keyboard::SetAccessibilityKeyboardEnabled(enabled);
  // Note that there are two versions of the on-screen keyboard. A full layout
  // is provided for accessibility, which includes sticky modifier keys to
  // enable typing of hotkeys. A compact version is used in touchview mode
  // to provide a layout with larger keys to facilitate touch typing. In the
  // event that the a11y keyboard is being disabled, an on-screen keyboard might
  // still be enabled and a forced reset is required to pick up the layout
  // change.
  if (keyboard::IsKeyboardEnabled())
    ash::Shell::GetInstance()->CreateKeyboard();
  else
    ash::Shell::GetInstance()->DeactivateKeyboard();
#endif
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
    scoped_ptr<extensions::api::braille_display_private::DisplayState> state) {
  OnBrailleDisplayStateChanged(*state);
}

void AccessibilityManager::UpdateBrailleImeState() {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  std::vector<std::string> preload_engines;
  base::SplitString(pref_service->GetString(prefs::kLanguagePreloadEngines),
                    ',',
                    &preload_engines);
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
                          JoinString(preload_engines, ','));
  braille_ime_current_ = false;
}

// Overridden from InputMethodManager::Observer.
void AccessibilityManager::InputMethodChanged(
    input_method::InputMethodManager* manager,
    bool show_message) {
#if defined(USE_ASH)
  // Sticky keys is implemented only in ash.
  ash::Shell::GetInstance()->sticky_keys_controller()->SetModifiersEnabled(
      manager->IsISOLevel5ShiftUsedByCurrentInputMethod(),
      manager->IsAltGrUsedByCurrentInputMethod());
#endif
  const chromeos::input_method::InputMethodDescriptor descriptor =
      manager->GetCurrentInputMethod();
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
  }

  large_cursor_pref_handler_.HandleProfileChanged(profile_, profile);
  spoken_feedback_pref_handler_.HandleProfileChanged(profile_, profile);
  high_contrast_pref_handler_.HandleProfileChanged(profile_, profile);
  autoclick_pref_handler_.HandleProfileChanged(profile_, profile);
  autoclick_delay_pref_handler_.HandleProfileChanged(profile_, profile);
  virtual_keyboard_pref_handler_.HandleProfileChanged(profile_, profile);

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
}

void AccessibilityManager::ActiveUserChanged(const std::string& user_id) {
  SetProfile(ProfileManager::GetActiveUserProfile());
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
  if (!ash::PlaySystemSoundIfSpokenFeedback(SOUND_SHUTDOWN))
    return base::TimeDelta();
  return media::SoundsManager::Get()->GetDuration(SOUND_SHUTDOWN);
}

void AccessibilityManager::InjectChromeVox(RenderViewHost* render_view_host) {
  LoadChromeVoxExtension(profile_, render_view_host, base::Closure());
}

scoped_ptr<AccessibilityStatusSubscription>
    AccessibilityManager::RegisterCallback(
        const AccessibilityStatusCallback& cb) {
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
    uint32 type = MagnificationManager::Get()->IsMagnifierEnabled() ?
                      MagnificationManager::Get()->GetMagnifierType() : 0;
    // '0' means magnifier is disabled.
    UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosScreenMagnifier",
                              type,
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
          LoadChromeVoxToLockScreen(base::Closure());
        // If spoken feedback was enabled, make sure it is also enabled on
        // the user screen.
        // The status tray gets verbalized by user screen ChromeVox, so we need
        // to load it on the user screen even if the screen is locked.
        LoadChromeVoxToUserScreen(base::Closure());
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
      braille_display_connected_,
      ash::A11Y_NOTIFICATION_SHOW);
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::OnBrailleKeyEvent(const KeyEvent& event) {
  // Ensure the braille IME is active on braille keyboard (dots) input.
  if ((event.command ==
       extensions::api::braille_display_private::KEY_COMMAND_DOTS) &&
      !braille_ime_current_) {
    input_method::InputMethodManager::Get()->ChangeInputMethod(
        extension_misc::kBrailleImeEngineId);
  }
}

void AccessibilityManager::PostLoadChromeVox(Profile* profile) {
  // Do any setup work needed immediately after ChromeVox actually loads.
  if (system_sounds_enabled_)
    ash::PlaySystemSoundAlways(SOUND_SPOKEN_FEEDBACK_ENABLED);

  ExtensionAccessibilityEventRouter::GetInstance()->
      OnChromeVoxLoadStateChanged(profile_,
          IsSpokenFeedbackEnabled(),
          chrome_vox_loaded_on_lock_screen_ ||
              should_speak_chrome_vox_announcements_on_user_screen_);

  should_speak_chrome_vox_announcements_on_user_screen_ =
      chrome_vox_loaded_on_lock_screen_;
}

void AccessibilityManager::PostUnloadChromeVox(Profile* profile) {
  // Do any teardown work needed immediately after ChromeVox actually unloads.
  if (system_sounds_enabled_)
    ash::PlaySystemSoundAlways(SOUND_SPOKEN_FEEDBACK_DISABLED);
}

}  // namespace chromeos

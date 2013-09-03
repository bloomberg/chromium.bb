// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/event_rewriter_event_filter.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/file_reader.h"
#include "extensions/common/extension_resource.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::RenderViewHost;

namespace chromeos {

namespace {

static chromeos::AccessibilityManager* g_accessibility_manager = NULL;

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

  // Fianlly, call this method once to fetch all of the resources and
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

void LoadChromeVoxExtension(Profile* profile, content::WebUI* login_web_ui) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  base::FilePath path = base::FilePath(extension_misc::kChromeVoxExtensionPath);
  std::string extension_id =
      extension_service->component_loader()->Add(IDR_CHROMEVOX_MANIFEST,
                                                 path);
  if (login_web_ui) {
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    const extensions::Extension* extension =
        extension_service->extensions()->GetByID(extension_id);

    RenderViewHost* render_view_host =
        login_web_ui->GetWebContents()->GetRenderViewHost();
    // Set a flag to tell ChromeVox that it's just been enabled,
    // so that it won't interrupt our speech feedback enabled message.
    ExtensionMsg_ExecuteCode_Params params;
    params.request_id = 0;
    params.extension_id = extension->id();
    params.is_javascript = true;
    params.code = "window.INJECTED_AFTER_LOAD = true;";
    params.run_at = extensions::UserScript::DOCUMENT_IDLE;
    params.all_frames = true;
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
        const extensions::UserScript::File &file = script.js_scripts()[j];
        extensions::ExtensionResource resource = extension->GetResource(
            file.relative_path());
        loader->AppendScript(resource);
      }
    }
    loader->Run();  // It cleans itself up when done.
  }
  DLOG(INFO) << "ChromeVox was Loaded.";
}

void UnloadChromeVoxExtension(Profile* profile) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  base::FilePath path = base::FilePath(extension_misc::kChromeVoxExtensionPath);
  extension_service->component_loader()->Remove(path);
  DLOG(INFO) << "ChromeVox was Unloaded.";
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// AccessibilityStatusEventDetails

AccessibilityStatusEventDetails::AccessibilityStatusEventDetails(
    bool enabled,
    ash::AccessibilityNotificationVisibility notify)
  : enabled(enabled),
    magnifier_type(ash::kDefaultMagnifierType),
    notify(notify) {}

AccessibilityStatusEventDetails::AccessibilityStatusEventDetails(
    bool enabled,
    ash::MagnifierType magnifier_type,
    ash::AccessibilityNotificationVisibility notify)
  : enabled(enabled),
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
      large_cursor_pref_handler_(prefs::kLargeCursorEnabled),
      spoken_feedback_pref_handler_(prefs::kSpokenFeedbackEnabled),
      high_contrast_pref_handler_(prefs::kHighContrastEnabled),
      large_cursor_enabled_(false),
      sticky_keys_enabled_(false),
      spoken_feedback_enabled_(false),
      high_contrast_enabled_(false),
      spoken_feedback_notification_(ash::A11Y_NOTIFICATION_NONE) {

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
}

AccessibilityManager::~AccessibilityManager() {
  CHECK(this == g_accessibility_manager);
}

void AccessibilityManager::EnableLargeCursor(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kLargeCursorEnabled, enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::UpdateLargeCursorFromPref() {
  if (!profile_)
    return;

  const bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kLargeCursorEnabled);

  if (large_cursor_enabled_ == enabled)
    return;

  large_cursor_enabled_ = enabled;

  AccessibilityStatusEventDetails details(enabled, ash::A11Y_NOTIFICATION_NONE);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_LARGE_CURSOR,
      content::NotificationService::AllSources(),
      content::Details<AccessibilityStatusEventDetails>(&details));

#if defined(USE_ASH)
  // Large cursor is implemented only in ash.
  ash::Shell::GetInstance()->cursor_manager()->SetCursorSet(
      enabled ? ui::CURSOR_SET_LARGE : ui::CURSOR_SET_NORMAL);
#endif
}

bool AccessibilityManager::IsLargeCursorEnabled() {
  return large_cursor_enabled_;
}

void AccessibilityManager::EnableStickyKeys(bool enabled) {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kStickyKeysEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsStickyKeysEnabled() {
  return sticky_keys_enabled_;
}

void AccessibilityManager::UpdateStickyKeysFromPref() {
  if (!profile_)
    return;

  const bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kStickyKeysEnabled);

  if (sticky_keys_enabled_ == enabled)
    return;

  sticky_keys_enabled_ = enabled;
#if defined(USE_ASH)
  // Sticky keys is implemented only in ash.
  ash::Shell::GetInstance()->event_rewriter_filter()->EnableStickyKeys(enabled);
#endif
}

void AccessibilityManager::EnableSpokenFeedback(
    bool enabled,
    ash::AccessibilityNotificationVisibility notify) {
  if (!profile_)
    return;

  spoken_feedback_notification_ = notify;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(
      prefs::kSpokenFeedbackEnabled, enabled);
  pref_service->CommitPendingWrite();

  spoken_feedback_notification_ = ash::A11Y_NOTIFICATION_NONE;
}

void AccessibilityManager::UpdateSpokenFeedbackFromPref() {
  if (!profile_)
    return;

  const bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kSpokenFeedbackEnabled);

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  ExtensionAccessibilityEventRouter::GetInstance()->
      SetAccessibilityEnabled(enabled);

  AccessibilityStatusEventDetails details(enabled,
                                          spoken_feedback_notification_);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
      content::NotificationService::AllSources(),
      content::Details<AccessibilityStatusEventDetails>(&details));

  Speak(l10n_util::GetStringUTF8(
      enabled ? IDS_CHROMEOS_ACC_SPOKEN_FEEDBACK_ENABLED :
      IDS_CHROMEOS_ACC_SPOKEN_FEEDBACK_DISABLED).c_str());

  if (enabled)
    LoadChromeVox();
  else
    UnloadChromeVox();
}

void AccessibilityManager::LoadChromeVox() {
  ScreenLocker* screen_locker = ScreenLocker::default_screen_locker();
  if (screen_locker && screen_locker->locked()) {
    // If on the lock screen, loads ChromeVox only to the lock screen as for
    // now. On unlock, it will be loaded to the user screen.
    // (see. AccessibilityManager::Observe())
    LoadChromeVoxToLockScreen();
    return;
  }

  LoadChromeVoxToUserScreen();
}

void AccessibilityManager::LoadChromeVoxToUserScreen() {
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
  }

  LoadChromeVoxExtension(profile_, login_web_ui);
  chrome_vox_loaded_on_user_screen_ = true;
}

void AccessibilityManager::LoadChromeVoxToLockScreen() {
  if (chrome_vox_loaded_on_lock_screen_)
    return;

  ScreenLocker* screen_locker = ScreenLocker::default_screen_locker();
  if (screen_locker && screen_locker->locked()) {
    content::WebUI* lock_web_ui = screen_locker->GetAssociatedWebUI();
    if (lock_web_ui) {
      Profile* profile = Profile::FromWebUI(lock_web_ui);
      LoadChromeVoxExtension(profile, lock_web_ui);
      chrome_vox_loaded_on_lock_screen_ = true;
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

void AccessibilityManager::Speak(const std::string& text) {
  UtteranceContinuousParameters params;

  Utterance* utterance = new Utterance(profile_);
  utterance->set_text(text);
  utterance->set_lang(g_browser_process->GetApplicationLocale());
  utterance->set_continuous_parameters(params);
  utterance->set_can_enqueue(false);
  utterance->set_options(new DictionaryValue());

  TtsController* controller = TtsController::GetInstance();
  controller->SpeakOrEnqueue(utterance);
}

void AccessibilityManager::MaybeSpeak(const std::string& text) {
  if (IsSpokenFeedbackEnabled())
    Speak(text);
}

void AccessibilityManager::EnableHighContrast(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kHighContrastEnabled, enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::UpdateHighContrastFromPref() {
  if (!profile_)
    return;

  const bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kHighContrastEnabled);

  if (high_contrast_enabled_ == enabled)
    return;

  high_contrast_enabled_ = enabled;

  AccessibilityStatusEventDetails detail(enabled, ash::A11Y_NOTIFICATION_NONE);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE,
      content::NotificationService::AllSources(),
      content::Details<AccessibilityStatusEventDetails>(&detail));

#if defined(USE_ASH)
  ash::Shell::GetInstance()->high_contrast_controller()->SetEnabled(enabled);
#endif
}

void AccessibilityManager::LocalePrefChanged() {
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

bool AccessibilityManager::IsHighContrastEnabled() {
  return high_contrast_enabled_;
}

void AccessibilityManager::SetProfile(Profile* profile) {
  pref_change_registrar_.reset();
  local_state_pref_change_registrar_.reset();

  if (profile) {
    // TODO(yoshiki): Move following code to PrefHandler.
    pref_change_registrar_.reset(new PrefChangeRegistrar);
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        prefs::kLargeCursorEnabled,
        base::Bind(&AccessibilityManager::UpdateLargeCursorFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kStickyKeysEnabled,
        base::Bind(&AccessibilityManager::UpdateStickyKeysFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kSpokenFeedbackEnabled,
        base::Bind(&AccessibilityManager::UpdateSpokenFeedbackFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kHighContrastEnabled,
        base::Bind(&AccessibilityManager::UpdateHighContrastFromPref,
                   base::Unretained(this)));

    local_state_pref_change_registrar_.reset(new PrefChangeRegistrar);
    local_state_pref_change_registrar_->Init(g_browser_process->local_state());
    local_state_pref_change_registrar_->Add(
        prefs::kApplicationLocale,
        base::Bind(&AccessibilityManager::LocalePrefChanged,
                   base::Unretained(this)));

    content::BrowserAccessibilityState::GetInstance()->AddHistogramCallback(
        base::Bind(
            &AccessibilityManager::UpdateChromeOSAccessibilityHistograms,
            base::Unretained(this)));
  }

  large_cursor_pref_handler_.HandleProfileChanged(profile_, profile);
  spoken_feedback_pref_handler_.HandleProfileChanged(profile_, profile);
  high_contrast_pref_handler_.HandleProfileChanged(profile_, profile);

  profile_ = profile;
  UpdateLargeCursorFromPref();
  UpdateStickyKeysFromPref();
  UpdateSpokenFeedbackFromPref();
  UpdateHighContrastFromPref();
}

void AccessibilityManager::SetProfileForTest(Profile* profile) {
  SetProfile(profile);
}

void AccessibilityManager::UpdateChromeOSAccessibilityHistograms() {
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosSpokenFeedback",
                        IsSpokenFeedbackEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosHighContrast",
                        IsHighContrastEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosVirtualKeyboard",
                        accessibility::IsVirtualKeyboardEnabled());
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
    UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosLargeCursor",
                          prefs->GetBoolean(prefs::kLargeCursorEnabled));
    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.CrosAlwaysShowA11yMenu",
        prefs->GetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu));
  }
}

void AccessibilityManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
      // Update |profile_| when entering the login screen.
      Profile* profile = ProfileManager::GetDefaultProfile();
      if (ProfileHelper::IsSigninProfile(profile))
        SetProfile(profile);
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED:
      // Update |profile_| when entering a session.
      SetProfile(ProfileManager::GetDefaultProfile());
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
      if (is_screen_locked) {
        if (spoken_feedback_enabled_)
          LoadChromeVoxToLockScreen();
      } else {
        UnloadChromeVoxFromLockScreen();

        if (spoken_feedback_enabled_)
          LoadChromeVoxToUserScreen();
      }
    }
  }
}

}  // namespace chromeos

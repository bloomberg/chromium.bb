// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include "chrome/browser/about_flags.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/background_page_tracker.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/policy_content_settings_provider.h"
#include "chrome/browser/content_settings/pref_content_settings_provider.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_ui/flags_ui.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/dom_ui/plugins_ui.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_prefs.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/page_info_model.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/policy/profile_policy_context.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/keyword_editor_controller.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"

#if defined(TOOLKIT_VIEWS)  // TODO(port): whittle this down as we port
#include "chrome/browser/ui/views/browser_actions_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/audio_mixer_alsa.h"
#include "chrome/browser/chromeos/login/apply_services_customization.h"
#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#endif

namespace browser {

void RegisterAllPrefs(PrefService* user_prefs, PrefService* local_state) {
  RegisterLocalState(local_state);
  RegisterUserPrefs(user_prefs);
}

void RegisterLocalState(PrefService* local_state) {
  // Prefs in Local State
  Browser::RegisterPrefs(local_state);
  WebCacheManager::RegisterPrefs(local_state);
  ExternalProtocolHandler::RegisterPrefs(local_state);
  GoogleURLTracker::RegisterPrefs(local_state);
  IntranetRedirectDetector::RegisterPrefs(local_state);
  KeywordEditorController::RegisterPrefs(local_state);
  MetricsLog::RegisterPrefs(local_state);
  MetricsService::RegisterPrefs(local_state);
  SafeBrowsingService::RegisterPrefs(local_state);
  browser_shutdown::RegisterPrefs(local_state);
#if defined(TOOLKIT_VIEWS)
  BrowserView::RegisterBrowserViewPrefs(local_state);
#endif
  UpgradeDetector::RegisterPrefs(local_state);
  TaskManager::RegisterPrefs(local_state);
  geolocation::RegisterPrefs(local_state);
  AutoFillManager::RegisterBrowserPrefs(local_state);
  BackgroundPageTracker::RegisterPrefs(local_state);
  NotificationUIManager::RegisterPrefs(local_state);
#if defined(OS_CHROMEOS)
  chromeos::AudioMixerAlsa::RegisterPrefs(local_state);
  chromeos::UserManager::RegisterPrefs(local_state);
  chromeos::UserCrosSettingsProvider::RegisterPrefs(local_state);
  WizardController::RegisterPrefs(local_state);
  chromeos::InputMethodMenuButton::RegisterPrefs(local_state);
  chromeos::ApplyServicesCustomization::RegisterPrefs(local_state);
  chromeos::SignedSettingsTempStorage::RegisterPrefs(local_state);
#endif
}

void RegisterUserPrefs(PrefService* user_prefs) {
  // User prefs
  AutoFillManager::RegisterUserPrefs(user_prefs);
  SessionStartupPref::RegisterUserPrefs(user_prefs);
  Browser::RegisterUserPrefs(user_prefs);
  PasswordManager::RegisterUserPrefs(user_prefs);
  chrome_browser_net::RegisterUserPrefs(user_prefs);
  DownloadPrefs::RegisterUserPrefs(user_prefs);
  bookmark_utils::RegisterUserPrefs(user_prefs);
  TabContents::RegisterUserPrefs(user_prefs);
  TemplateURLPrepopulateData::RegisterUserPrefs(user_prefs);
  ExtensionDOMUI::RegisterUserPrefs(user_prefs);
  ExtensionsUI::RegisterUserPrefs(user_prefs);
  FlagsUI::RegisterUserPrefs(user_prefs);
  NewTabUI::RegisterUserPrefs(user_prefs);
  PluginsUI::RegisterUserPrefs(user_prefs);
  ProfileImpl::RegisterUserPrefs(user_prefs);
  HostContentSettingsMap::RegisterUserPrefs(user_prefs);
  PolicyContentSettingsProvider::RegisterUserPrefs(user_prefs);
  PrefContentSettingsProvider::RegisterUserPrefs(user_prefs);
  HostZoomMap::RegisterUserPrefs(user_prefs);
  DevToolsManager::RegisterUserPrefs(user_prefs);
  PinnedTabCodec::RegisterUserPrefs(user_prefs);
  ExtensionPrefs::RegisterUserPrefs(user_prefs);
  GeolocationContentSettingsMap::RegisterUserPrefs(user_prefs);
  TranslatePrefs::RegisterUserPrefs(user_prefs);
  DesktopNotificationService::RegisterUserPrefs(user_prefs);
  PrefProxyConfigService::RegisterUserPrefs(user_prefs);
#if defined(TOOLKIT_VIEWS)
  BrowserActionsContainer::RegisterUserPrefs(user_prefs);
#elif defined(TOOLKIT_GTK)
  BrowserWindowGtk::RegisterUserPrefs(user_prefs);
#endif
#if defined(OS_CHROMEOS)
  chromeos::Preferences::RegisterUserPrefs(user_prefs);
#endif
  BackgroundContentsService::RegisterUserPrefs(user_prefs);
  SigninManager::RegisterUserPrefs(user_prefs);
  TemplateURLModel::RegisterUserPrefs(user_prefs);
  InstantController::RegisterUserPrefs(user_prefs);
  NetPrefObserver::RegisterPrefs(user_prefs);
  policy::ProfilePolicyContext::RegisterUserPrefs(user_prefs);
}

void MigrateBrowserPrefs(PrefService* user_prefs, PrefService* local_state) {
  // Copy pref values which have been migrated to user_prefs from local_state,
  // or remove them from local_state outright, if copying is not required.
  int current_version =
      local_state->GetInteger(prefs::kMultipleProfilePrefMigration);

  if ((current_version & WINDOWS_PREFS) == 0) {
    // Migrate the devtools split location preference.
    local_state->RegisterIntegerPref(prefs::kDevToolsSplitLocation, -1);
    DCHECK(user_prefs->FindPreference(prefs::kDevToolsSplitLocation));
    if (local_state->HasPrefPath(prefs::kDevToolsSplitLocation)) {
      user_prefs->SetInteger(prefs::kDevToolsSplitLocation,
          local_state->GetInteger(prefs::kDevToolsSplitLocation));
    }
    local_state->ClearPref(prefs::kDevToolsSplitLocation);

    // Migrate the browser window placement preference.
    local_state->RegisterDictionaryPref(prefs::kBrowserWindowPlacement);
    DCHECK(user_prefs->FindPreference(prefs::kBrowserWindowPlacement));
    if (local_state->HasPrefPath(prefs::kBrowserWindowPlacement)) {
      user_prefs->Set(prefs::kBrowserWindowPlacement,
          *(local_state->FindPreference(prefs::kBrowserWindowPlacement)->
              GetValue()->DeepCopy()));
    }
    local_state->ClearPref(prefs::kBrowserWindowPlacement);

    local_state->SetInteger(prefs::kMultipleProfilePrefMigration,
                            current_version | WINDOWS_PREFS);
  }
}

}  // namespace browser

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/features.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync/driver/sync_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "printing/features/features.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/user_manager/user_manager.h"
#else  // defined(OS_CHROMEOS)
#include "chrome/browser/shell_integration.h"
#endif  // !defined(OS_CHROMEOS)

class TemplateURLService;

namespace base {
class Value;
}

namespace policy {
class PolicyChangeRegistrar;
}

namespace options {

// Chrome browser options page UI handler.
class BrowserOptionsHandler
    : public OptionsPageUIHandler,
      public ProfileAttributesStorage::Observer,
      public syncer::SyncServiceObserver,
      public SigninManagerBase::Observer,
      public ui::SelectFileDialog::Listener,
#if defined(OS_CHROMEOS)
      public chromeos::system::PointerDeviceObserver::Observer,
      public ArcAppListPrefs::Observer,
      public user_manager::UserManager::Observer,
#endif
      public TemplateURLServiceObserver,
      public extensions::ExtensionRegistryObserver,
      public content::NotificationObserver,
      public policy::PolicyService::Observer {
 public:
  BrowserOptionsHandler();
  ~BrowserOptionsHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* values) override;
  void PageLoadStarted() override;
  void InitializeHandler() override;
  void InitializePage() override;
  void RegisterMessages() override;
  void Uninitialize() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  // SigninManagerBase::Observer implementation.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // TemplateURLServiceObserver implementation.
  void OnTemplateURLServiceChanged() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // policy::PolicyService::Observer:
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;
#if defined(OS_CHROMEOS)
  static void DisablePolymerPreloadForTesting();
#endif  // defined(OS_CHROMEOS)

 private:
  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ProfileAttributesStorage::Observer implementation.
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
  void OnCloudPrintPrefsChanged();
#endif

  // SelectFileDialog::Listener implementation
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

#if defined(OS_CHROMEOS)
  // PointerDeviceObserver::Observer implementation.
  void TouchpadExists(bool exists) override;
  void MouseExists(bool exists) override;

  // Will be called when the policy::key::kUserAvatarImage policy changes.
  void OnUserImagePolicyChanged(const base::Value* previous_policy,
                                const base::Value* current_policy);

  // Will be called when the policy::key::kWallpaperImage policy changes.
  void OnWallpaperPolicyChanged(const base::Value* previous_policy,
                                const base::Value* current_policy);

  // Will be called when powerwash dialog is shown.
  void OnPowerwashDialogShow(const base::ListValue* args);

  // ArcAppListPrefs::Observer overrides.
  void OnAppReadyChanged(const std::string& app_id, bool ready) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;

  // user_manager::UserManager::Observer overrides.
  void OnUserImageChanged(const user_manager::User& user) override;
#endif

  void UpdateSyncState();

  // Will be called when the kSigninAllowed pref has changed.
  void OnSigninAllowedPrefChange();

  // Sets the search engine at the given index to be default. Called from WebUI.
  void SetDefaultSearchEngine(const base::ListValue* args);

  // Returns if the "make Chrome default browser" button should be shown.
  bool ShouldShowSetDefaultBrowser();

  // Returns if profiles list should be shown on settings page.
  bool ShouldShowMultiProfilesUserList();

  // Returns if access to advanced settings should be allowed.
  bool ShouldAllowAdvancedSettings();

#if !defined(OS_CHROMEOS)
  // Gets the current default browser state, and asynchronously reports it to
  // the WebUI page.
  void UpdateDefaultBrowserState();

  // Makes this the default browser. Called from WebUI.
  void BecomeDefaultBrowser(const base::ListValue* args);

  // Receives the default browser state when the worker is done.
  void OnDefaultBrowserWorkerFinished(
      shell_integration::DefaultWebClientState state);

  // Updates the UI with the given state for the default browser.
  void SetDefaultBrowserUIString(int status_string_id);
#endif  // !defined(OS_CHROMEOS)

  // Loads the possible default search engine list and reports it to the WebUI.
  void AddTemplateUrlServiceObserver();

  // Creates a list of dictionaries where each dictionary is of the form:
  //   profileInfo = {
  //     name: "Profile Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     filePath: "/path/to/profile/data/on/disk",
  //     isCurrentProfile: false
  //   };
  std::unique_ptr<base::ListValue> GetProfilesInfoList();

  // Sends an array of Profile objects to javascript.
  void SendProfilesInfo();

  // Deletes the given profile. Expects one argument:
  //   0: profile file path (string)
  void DeleteProfile(const base::ListValue* args);

  void ObserveThemeChanged();
  void ThemesReset(const base::ListValue* args);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  void ThemesSetNative(const base::ListValue* args);
#endif

#if defined(OS_CHROMEOS)
  void UpdateAccountPicture();

  // Updates the UI, allowing the user to change the avatar image if |managed|
  // is |false| and preventing the user from changing the avatar image if
  // |managed| is |true|.
  void OnAccountPictureManagedChanged(bool managed);

  // Updates the UI, allowing the user to change the wallpaper if |managed| is
  // |false| and preventing the user from changing the wallpaper if |managed| is
  // |true|.
  void OnWallpaperManagedChanged(bool managed);

  // Updates the UI, allowing the user to change the system time zone if
  // kSystemTimezonePolicy is set, and preventing the user from changing the
  // system time zone if kSystemTimezonePolicy is not set.
  void OnSystemTimezonePolicyChanged();

  // Updates the UI, preventing the user from changing timezone or timezone
  // detection settings if kSystemTimezoneAutomaticDetectionPolicy is set, and
  // allowing the user to update these settings if
  // kSystemTimezoneAutomaticDetectionPolicy is not set.
  void OnSystemTimezoneAutomaticDetectionPolicyChanged();
#endif

  // Callback for the "selectDownloadLocation" message. This will prompt the
  // user for a destination folder using platform-specific APIs.
  void HandleSelectDownloadLocation(const base::ListValue* args);

  // Callback for the "autoOpenFileTypesResetToDefault" message. This will
  // remove all auto-open file-type settings.
  void HandleAutoOpenButton(const base::ListValue* args);

  // Callback for the "defaultFontSizeAction" message. This is called if the
  // user changes the default font size. |args| is an array that contains
  // one item, the font size as a numeric value.
  void HandleDefaultFontSize(const base::ListValue* args);

  // Callback for the "defaultZoomFactorAction" message. This is called if the
  // user changes the default zoom factor. |args| is an array that contains
  // one item, the zoom factor as a numeric value.
  void HandleDefaultZoomFactor(const base::ListValue* args);

  // Callback for the "Use SSL 3.0" checkbox. This is called if the user toggles
  // the "Use SSL 3.0" checkbox.
  void HandleUseSSL3Checkbox(const base::ListValue* args);

  // Callback for the "Use TLS 1.0" checkbox. This is called if the user toggles
  // the "Use TLS 1.0" checkbox.
  void HandleUseTLS1Checkbox(const base::ListValue* args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const base::ListValue* args);

  // Callback for "requestProfilesInfo" message.
  void HandleRequestProfilesInfo(const base::ListValue* args);

#if !defined(OS_CHROMEOS)
  // Callback for the "showNetworkProxySettings" message. This will invoke
  // an appropriate dialog for configuring proxy settings.
  void ShowNetworkProxySettings(const base::ListValue* args);
#endif

#if !defined(USE_NSS_CERTS)
  // Callback for the "showManageSSLCertificates" message. This will invoke
  // an appropriate certificate management action based on the platform.
  void ShowManageSSLCertificates(const base::ListValue* args);
#endif

#if defined(OS_CHROMEOS)
  void ShowCupsPrintDevicesPage(const base::ListValue* args);
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  void ShowCloudPrintDevicesPage(const base::ListValue* args);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Register localized values used by Cloud Print
  void RegisterCloudPrintValues(base::DictionaryValue* values);
#endif

  // Check if hotword is available. If it is, tell the javascript to show
  // the hotword section of the settings page.
  void SendHotwordAvailable();

  // Callback that updates the visibility of the audio history upon completion
  // of a call to the server to the get the current value.
  void SetHotwordAudioHistorySectionVisible(
      const base::string16& audio_history_state,
      bool success,
      bool logging_enabled);

  // Callback for "requestHotwordAvailable" message.
  void HandleRequestHotwordAvailable(const base::ListValue* args);

  // Callback for "launchHotwordAudioVerificationApp" message.
  void HandleLaunchHotwordAudioVerificationApp(const base::ListValue* args);

  // Callback for "requestGoogleNowAvailable" message.
  void HandleRequestGoogleNowAvailable(const base::ListValue* args);

  // Callback for "launchEasyUnlockSetup" message.
  void HandleLaunchEasyUnlockSetup(const base::ListValue* args);

  // Callback for "refreshExtensionControlIndicators" message.
  void HandleRefreshExtensionControlIndicators(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  // Opens the wallpaper manager component extension.
  void HandleOpenWallpaperManager(const base::ListValue* args);

  // Called when the accessibility checkbox values are changed.
  // |args| will contain the checkbox checked state as a string
  // ("true" or "false").
  void VirtualKeyboardChangeCallback(const base::ListValue* args);

  // Called when the user confirmed factory reset. Chrome will
  // initiate asynchronous file operation and then log out.
  void PerformFactoryResetRestart(const base::ListValue* args);

  // Update visibility of Android apps settings section.
  void UpdateAndroidSettingsAppState(bool visible);

  // Called to show Android apps settings.
  void ShowAndroidAppsSettings(const base::ListValue* args);

  // Called to show apps based on a url for the Play Store.
  void ShowPlayStoreApps(const base::ListValue* args);

  // Called to show TalkBack settings.
  void ShowAccessibilityTalkBackSettings(const base::ListValue *args);
#endif

  // Setup the visibility for the metrics reporting setting.
  void SetupMetricsReportingSettingVisibility();

  // Update value of predictive network actions UI element.
  void SetupNetworkPredictionControl();

  // Setup the font size selector control.
  void SetupFontSizeSelector();

  // Setup the page zoom selector control.
  void SetupPageZoomSelector();

  // Setup the visibility of the reset button.
  void SetupAutoOpenFileTypes();

  // Setup the proxy settings section UI.
  void SetupProxySettingsSection();

  // Setup the UI specific to managing supervised users.
  void SetupManagingSupervisedUsers();

  // Setup the UI for Easy Unlock.
  void SetupEasyUnlock();

  // Setup the UI for showing which settings are extension controlled.
  void SetupExtensionControlledIndicators();

  // Setup the value and the disabled property for metrics reporting for (except
  // Android).
  void SetupMetricsReportingCheckbox();

  // Called when the MetricsReportingEnabled checkbox values are changed.
  // |args| will contain the checkbox checked state as a boolean.
  void HandleMetricsReportingChange(const base::ListValue* args);

  // Notifies the result of MetricsReportingEnabled change to Javascript layer.
  void NotifyUIOfMetricsReportingChange(bool enabled);

  // Calls a Javascript function to set the state of MetricsReporting checkbox.
  void SetMetricsReportingCheckbox(bool checked,
                                   bool policy_managed,
                                   bool owner_managed);

#if defined(OS_CHROMEOS)
  // Setup the accessibility features for ChromeOS.
  void SetupAccessibilityFeatures();
#endif

  // Update the Extended Reporting Enabled checkbox based on the current state
  // of the opt-in.
  void SetupSafeBrowsingExtendedReporting();

  // Callback for "safeBrowsingExtentedReportingAction" message. This is called
  // if the user checks or unchecks the Extended Reporting checkbox. |args| is
  // an array that contains one item, the checked state of the checkbox.
  void HandleSafeBrowsingExtendedReporting(const base::ListValue* args);

  // Returns a newly created dictionary with a number of properties that
  // correspond to the status of sync.
  std::unique_ptr<base::DictionaryValue> GetSyncStateDictionary();

  // Checks whether on Chrome OS the current user is the device owner. Returns
  // true on other platforms.
  bool IsDeviceOwnerProfile();

#if !defined(OS_CHROMEOS)
  scoped_refptr<shell_integration::DefaultBrowserWorker>
      default_browser_worker_;
  BooleanPrefMember default_browser_policy_;
#endif

  bool page_initialized_;

  StringPrefMember homepage_;

  TemplateURLService* template_url_service_;  // Weak.

  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;

  bool cloud_print_mdns_ui_enabled_;

  StringPrefMember auto_open_files_;

  std::unique_ptr<ChromeZoomLevelPrefs::DefaultZoomLevelSubscription>
      default_zoom_level_subscription_;

  PrefChangeRegistrar profile_pref_registrar_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<policy::PolicyChangeRegistrar> policy_registrar_;

  // Whether factory reset can be performed.
  bool enable_factory_reset_;

  PrefChangeRegistrar local_state_pref_change_registrar_;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      system_timezone_policy_observer_;
#endif

  ScopedObserver<SigninManagerBase, SigninManagerBase::Observer>
      signin_observer_;

  // Used to get WeakPtr to self for use on the UI thread.
  base::WeakPtrFactory<BrowserOptionsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_

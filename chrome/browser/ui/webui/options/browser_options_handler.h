// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_member.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/notification_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#endif  // defined(OS_CHROMEOS)

class AutocompleteController;
class CloudPrintSetupHandler;
class CustomHomePagesTableModel;
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
      public ProfileSyncServiceObserver,
      public SigninManagerBase::Observer,
      public ui::SelectFileDialog::Listener,
      public ShellIntegration::DefaultWebClientObserver,
#if defined(OS_CHROMEOS)
      public chromeos::system::PointerDeviceObserver::Observer,
      public policy::ConsumerManagementService::Observer,
#endif
      public TemplateURLServiceObserver,
      public extensions::ExtensionRegistryObserver,
      public content::NotificationObserver {
 public:
  BrowserOptionsHandler();
  virtual ~BrowserOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* values) OVERRIDE;
  virtual void PageLoadStarted() OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;
  virtual void Uninitialize() OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // SigninManagerBase::Observer implementation.
  virtual void GoogleSigninSucceeded(const std::string& account_id,
                                     const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& account_id,
                               const std::string& username) OVERRIDE;

  // ShellIntegration::DefaultWebClientObserver implementation.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE;
  virtual bool IsInteractiveSetDefaultPermitted() OVERRIDE;

  // TemplateURLServiceObserver implementation.
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

  // extensions::ExtensionRegistryObserver:
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

#if defined(ENABLE_FULL_PRINTING) && !defined(OS_CHROMEOS)
  void OnCloudPrintPrefsChanged();
#endif

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE;

#if defined(OS_CHROMEOS)
  // PointerDeviceObserver::Observer implementation.
  virtual void TouchpadExists(bool exists) OVERRIDE;
  virtual void MouseExists(bool exists) OVERRIDE;

  // Will be called when the policy::key::kUserAvatarImage policy changes.
  void OnUserImagePolicyChanged(const base::Value* previous_policy,
                                const base::Value* current_policy);

  // Will be called when the policy::key::kWallpaperImage policy changes.
  void OnWallpaperPolicyChanged(const base::Value* previous_policy,
                                const base::Value* current_policy);

  // Will be called when powerwash dialog is shown.
  void OnPowerwashDialogShow(const base::ListValue* args);

  // ConsumerManagementService::Observer:
  virtual void OnConsumerManagementStatusChanged() OVERRIDE;
#endif

  void UpdateSyncState();

  // Will be called when the kSigninAllowed pref has changed.
  void OnSigninAllowedPrefChange();

  // Makes this the default browser. Called from WebUI.
  void BecomeDefaultBrowser(const base::ListValue* args);

  // Sets the search engine at the given index to be default. Called from WebUI.
  void SetDefaultSearchEngine(const base::ListValue* args);

  // Enables/disables auto-launching of Chrome on computer startup.
  void ToggleAutoLaunch(const base::ListValue* args);

  // Checks (on the file thread) whether the user is in the auto-launch trial
  // and whether Chrome is set to auto-launch at login. Gets a reply on the UI
  // thread (see CheckAutoLaunchCallback). A weak pointer to this is passed in
  // as a parameter to avoid the need to lock between this function and the
  // destructor. |profile_path| is the full path to the current profile.
  static void CheckAutoLaunch(base::WeakPtr<BrowserOptionsHandler> weak_this,
                              const base::FilePath& profile_path);

  // Sets up (on the UI thread) the necessary bindings for toggling auto-launch
  // (if the user is part of the auto-launch and makes sure the HTML UI knows
  // whether Chrome will auto-launch at login.
  void CheckAutoLaunchCallback(bool is_in_auto_launch_group,
                               bool will_launch_at_login);

  // Returns the string ID for the given default browser state.
  int StatusStringIdForState(ShellIntegration::DefaultWebClientState state);

  // Returns if the "make Chrome default browser" button should be shown.
  bool ShouldShowSetDefaultBrowser();

  // Returns if profiles list should be shown on settings page.
  bool ShouldShowMultiProfilesUserList();

  // Returns if access to advanced settings should be allowed.
  bool ShouldAllowAdvancedSettings();

  // Gets the current default browser state, and asynchronously reports it to
  // the WebUI page.
  void UpdateDefaultBrowserState();

  // Updates the UI with the given state for the default browser.
  void SetDefaultBrowserUIString(int status_string_id);

  // Loads the possible default search engine list and reports it to the WebUI.
  void AddTemplateUrlServiceObserver();

  // Creates a list of dictionaries where each dictionary is of the form:
  //   profileInfo = {
  //     name: "Profile Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     filePath: "/path/to/profile/data/on/disk",
  //     isCurrentProfile: false
  //   };
  scoped_ptr<base::ListValue> GetProfilesInfoList();

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

#if !defined(USE_NSS)
  // Callback for the "showManageSSLCertificates" message. This will invoke
  // an appropriate certificate management action based on the platform.
  void ShowManageSSLCertificates(const base::ListValue* args);
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
  void ShowCloudPrintDevicesPage(const base::ListValue* args);
#endif

#if defined(ENABLE_FULL_PRINTING)
  // Register localized values used by Cloud Print
  void RegisterCloudPrintValues(base::DictionaryValue* values);
#endif

  // Check if hotword is available. If it is, tell the javascript to show
  // the hotword section of the settings page.
  void SendHotwordAvailable();

  // Callback for "requestHotwordAvailable" message.
  void HandleRequestHotwordAvailable(const base::ListValue* args);

  // Callback for "launchHotwordAudioVerificationApp" message.
  void HandleLaunchHotwordAudioVerificationApp(const base::ListValue* args);

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

  // Setup the manage certificates section UI.
  void SetupManageCertificatesSection();

  // Setup the UI specific to managing supervised users.
  void SetupManagingSupervisedUsers();

  // Setup the UI for Easy Unlock.
  void SetupEasyUnlock();

  // Setup the UI for showing which settings are extension controlled.
  void SetupExtensionControlledIndicators();

  // Setup the value and the disabled property for metrics reporting for (except
  // CrOS and Android).
  void SetupMetricsReportingCheckbox();

  // Called when the MetricsReportingEnabled checkbox values are changed.
  // |args| will contain the checkbox checked state as a boolean.
  void HandleMetricsReportingChange(const base::ListValue* args);

  // Notifies the result of MetricsReportingEnabled change to Javascript layer.
  void MetricsReportingChangeCallback(bool enabled);

  // Calls a Javascript function to set the state of MetricsReporting checkbox.
  void SetMetricsReportingCheckbox(bool checked, bool disabled);

#if defined(OS_CHROMEOS)
  // Setup the accessibility features for ChromeOS.
  void SetupAccessibilityFeatures();
#endif

  // Returns a newly created dictionary with a number of properties that
  // correspond to the status of sync.
  scoped_ptr<base::DictionaryValue> GetSyncStateDictionary();

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  bool page_initialized_;

  StringPrefMember homepage_;
  BooleanPrefMember default_browser_policy_;

  TemplateURLService* template_url_service_;  // Weak.

  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;

  bool cloud_print_mdns_ui_enabled_;

  StringPrefMember auto_open_files_;
  DoublePrefMember default_zoom_level_;

  PrefChangeRegistrar profile_pref_registrar_;
#if defined(OS_CHROMEOS)
  scoped_ptr<policy::PolicyChangeRegistrar> policy_registrar_;
#endif

  ScopedObserver<SigninManagerBase, SigninManagerBase::Observer>
      signin_observer_;

  // Used to get WeakPtr to self for use on the UI thread.
  base::WeakPtrFactory<BrowserOptionsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_

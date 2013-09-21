// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_member.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#endif  // defined(OS_CHROMEOS)

class AutocompleteController;
class CloudPrintSetupHandler;
class CustomHomePagesTableModel;
class ManagedUserRegistrationUtility;
class TemplateURLService;

namespace options {

// Chrome browser options page UI handler.
class BrowserOptionsHandler
    : public OptionsPageUIHandler,
      public ProfileSyncServiceObserver,
      public ui::SelectFileDialog::Listener,
      public ShellIntegration::DefaultWebClientObserver,
#if defined(OS_CHROMEOS)
      public chromeos::system::PointerDeviceObserver::Observer,
#endif
      public TemplateURLServiceObserver {
 public:
  BrowserOptionsHandler();
  virtual ~BrowserOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* values) OVERRIDE;
  virtual void PageLoadStarted() OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // ShellIntegration::DefaultWebClientObserver implementation.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE;
  virtual bool IsInteractiveSetDefaultPermitted() OVERRIDE;

  // TemplateURLServiceObserver implementation.
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

 private:
  // Represents the final profile creation status. It is used to map
  // the status to the javascript method to be called.
  enum ProfileCreationStatus {
    PROFILE_CREATION_SUCCESS,
    PROFILE_CREATION_ERROR,
  };

  // Represents errors that could occur during a profile creation.
  // It is used to map error types to messages that will be displayed
  // to the user.
  enum ProfileCreationErrorType {
    REMOTE_ERROR,
    LOCAL_ERROR,
    SIGNIN_ERROR
  };

  // Represents the type of the in progress profile creation operation.
  // It is used to map the type of the profile creation operation to the
  // correct UMA metric name.
  enum ProfileCreationOperationType {
    SUPERVISED_PROFILE_CREATION,
    SUPERVISED_PROFILE_IMPORT,
    NON_SUPERVISED_PROFILE_CREATION,
    NO_CREATION_IN_PROGRESS,
  };

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
  scoped_ptr<ListValue> GetProfilesInfoList();

  // Sends an array of Profile objects to javascript.
  void SendProfilesInfo();

  // Returns the current desktop type.
  chrome::HostDesktopType GetDesktopType();

  // Asynchronously creates and initializes a new profile.
  // The arguments are as follows:
  //   0: name (string)
  //   1: icon (string)
  //   2: a flag stating whether we should create a profile desktop shortcut
  //      (optional, boolean)
  //   3: a flag stating whether the user should be managed (optional, boolean)
  void CreateProfile(const base::ListValue* args);

  // After a new managed-user profile has been created, registers the user with
  // the management server.
  void RegisterManagedUser(chrome::HostDesktopType desktop_type,
                           const std::string& managed_user_id,
                           Profile* new_profile);

  // Called back with the result of the managed user registration.
  void OnManagedUserRegistered(chrome::HostDesktopType desktop_type,
                               Profile* profile,
                               const GoogleServiceAuthError& error);

  // Records UMA histograms relevant to profile creation.
  void RecordProfileCreationMetrics(Profile::CreateStatus status);

  // Records UMA histograms relevant to supervised user profiles
  // creation and registration.
  void RecordSupervisedProfileCreationMetrics(
      GoogleServiceAuthError::State error_state);

  // If a local error occurs during profile creation, then show an appropriate
  // error message. However, if profile creation succeeded and the
  // profile being created/imported is a supervised user profile,
  // then proceed with the registration step. Otherwise, update the UI
  // as the final task after a new profile has been created.
  void FinalizeProfileCreation(chrome::HostDesktopType desktop_type,
                               const std::string& managed_user_id,
                               Profile* profile,
                               Profile::CreateStatus status);

  // Updates the UI to show an error when creating a profile.
  void ShowProfileCreationError(Profile* profile, const string16& error);

  // Updates the UI to show a non-fatal warning when creating a profile.
  void ShowProfileCreationWarning(const string16& warning);

  // Updates the UI to indicate success when creating a profile.
  void ShowProfileCreationSuccess(Profile* profile,
                                  chrome::HostDesktopType desktop_type);

  void HandleProfileCreationSuccess(Profile* profile,
                                    const std::string& managed_user_id,
                                    chrome::HostDesktopType desktop_type);

  // Deletes the given profile. Expects one argument:
  //   0: profile file path (string)
  void DeleteProfile(const base::ListValue* args);

  // Deletes the profile at the given |file_path|.
  void DeleteProfileAtPath(base::FilePath file_path);

  // Cancels creation of a managed-user profile currently in progress, as
  // indicated by profile_path_being_created_, removing the object and files
  // and canceling managed-user registration. This is the handler for the
  // "cancelCreateProfile" message. |args| is not used.
  // TODO(pamg): Move all the profile-handling methods into a more appropriate
  // class.
  void HandleCancelProfileCreation(const base::ListValue* args);

  // Internal implementation. This may safely be called whether profile creation
  // or registration is in progress or not. |user_initiated| should be true if
  // the cancellation was deliberately requested by the user, and false if it
  // was caused implicitly, e.g. by shutting down the browser.
  void CancelProfileRegistration(bool user_initiated);

  void ObserveThemeChanged();
  void ThemesReset(const base::ListValue* args);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  void ThemesSetNative(const base::ListValue* args);
#endif

#if defined(OS_CHROMEOS)
  void UpdateAccountPicture();
#endif

  // Callback for the "selectDownloadLocation" message. This will prompt the
  // user for a destination folder using platform-specific APIs.
  void HandleSelectDownloadLocation(const ListValue* args);

  // Callback for the "autoOpenFileTypesResetToDefault" message. This will
  // remove all auto-open file-type settings.
  void HandleAutoOpenButton(const ListValue* args);

  // Callback for the "defaultFontSizeAction" message. This is called if the
  // user changes the default font size. |args| is an array that contains
  // one item, the font size as a numeric value.
  void HandleDefaultFontSize(const ListValue* args);

  // Callback for the "defaultZoomFactorAction" message. This is called if the
  // user changes the default zoom factor. |args| is an array that contains
  // one item, the zoom factor as a numeric value.
  void HandleDefaultZoomFactor(const ListValue* args);

  // Callback for the "Use SSL 3.0" checkbox. This is called if the user toggles
  // the "Use SSL 3.0" checkbox.
  void HandleUseSSL3Checkbox(const ListValue* args);

  // Callback for the "Use TLS 1.0" checkbox. This is called if the user toggles
  // the "Use TLS 1.0" checkbox.
  void HandleUseTLS1Checkbox(const ListValue* args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const ListValue* args);

  // Callback for "requestProfilesInfo" message.
  void HandleRequestProfilesInfo(const ListValue* args);

#if !defined(OS_CHROMEOS)
  // Callback for the "showNetworkProxySettings" message. This will invoke
  // an appropriate dialog for configuring proxy settings.
  void ShowNetworkProxySettings(const ListValue* args);
#endif

#if !defined(USE_NSS)
  // Callback for the "showManageSSLCertificates" message. This will invoke
  // an appropriate certificate management action based on the platform.
  void ShowManageSSLCertificates(const ListValue* args);
#endif

#if defined(ENABLE_MDNS)
  void ShowCloudPrintDevicesPage(const ListValue* args);
#endif

#if defined(ENABLE_FULL_PRINTING)
  // Callback for the Cloud Print manage button. This will open a new
  // tab pointed at the management URL.
  void ShowCloudPrintManagePage(const ListValue* args);

  // Register localized values used by Cloud Print
  void RegisterCloudPrintValues(DictionaryValue* values);

#if !defined(OS_CHROMEOS)
  // Callback for the Sign in to Cloud Print button. This will start
  // the authentication process.
  void ShowCloudPrintSetupDialog(const ListValue* args);

  // Callback for the Disable Cloud Print button. This will sign out
  // of cloud print.
  void HandleDisableCloudPrintConnector(const ListValue* args);

  // Pings the service to send us it's current notion of the enabled state.
  void RefreshCloudPrintStatusFromService();

  // Setup the enabled or disabled state of the cloud print connector
  // management UI.
  void SetupCloudPrintConnectorSection();

  // Remove cloud print connector section if cloud print connector management
  //  UI is disabled.
  void RemoveCloudPrintConnectorSection();
#endif  // defined(OS_CHROMEOS)
#endif  // defined(ENABLE_FULL_PRINTING)

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

  // Setup the visibility for the password generation setting.
  void SetupPasswordGenerationSettingVisibility();

  // Setup the font size selector control.
  void SetupFontSizeSelector();

  // Setup the page zoom selector control.
  void SetupPageZoomSelector();

  // Setup the visibility of the reset button.
  void SetupAutoOpenFileTypes();

  // Setup the proxy settings section UI.
  void SetupProxySettingsSection();

#if defined(OS_CHROMEOS)
  // Setup the accessibility features for ChromeOS.
  void SetupAccessibilityFeatures();
#endif

  string16 GetProfileCreationErrorMessage(
      ProfileCreationErrorType error) const;
  std::string GetJavascriptMethodName(ProfileCreationStatus status) const;

  bool IsValidExistingManagedUserId(
      const std::string& existing_managed_user_id) const;

  // Returns a newly created dictionary with a number of properties that
  // correspond to the status of sync.
  scoped_ptr<DictionaryValue> GetSyncStateDictionary();

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  bool page_initialized_;

  StringPrefMember homepage_;
  BooleanPrefMember default_browser_policy_;

  TemplateURLService* template_url_service_;  // Weak.

  // Used to allow cancelling a profile creation (particularly a managed-user
  // registration) in progress. Set when profile creation is begun, and
  // cleared when all the callbacks have been run and creation is complete.
  base::FilePath profile_path_being_created_;

  // Used to track how long profile creation takes.
  base::TimeTicks profile_creation_start_time_;

  // Used to get WeakPtr to self for use on the UI thread.
  base::WeakPtrFactory<BrowserOptionsHandler> weak_ptr_factory_;

  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;

#if defined(ENABLE_FULL_PRINTING) && !defined(OS_CHROMEOS)
  StringPrefMember cloud_print_connector_email_;
  BooleanPrefMember cloud_print_connector_enabled_;
  bool cloud_print_connector_ui_enabled_;
#endif

  StringPrefMember auto_open_files_;
  DoublePrefMember default_zoom_level_;

  PrefChangeRegistrar profile_pref_registrar_;

  scoped_ptr<ManagedUserRegistrationUtility> managed_user_registration_utility_;

  // Indicates the type of the in progress profile creation operation.
  // The value is only relevant while we are creating/importing a profile.
  ProfileCreationOperationType profile_creation_type_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_

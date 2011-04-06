// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_ADVANCED_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_ADVANCED_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_handler.h"
#include "chrome/browser/remoting/remoting_options_handler.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class OptionsManagedBannerHandler;
class CloudPrintSetupHandler;

// Chrome advanced options page UI handler.
class AdvancedOptionsHandler
    : public OptionsPageUIHandler,
      public SelectFileDialog::Listener,
      public CloudPrintSetupHandlerDelegate {
 public:
  AdvancedOptionsHandler();
  virtual ~AdvancedOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const FilePath& path, int index, void* params);

  // CloudPrintSetupHandler::Delegate implementation.
  virtual void OnCloudPrintSetupClosed();

 private:
  // Callback for the "selectDownloadLocation" message.  This will prompt
  // the user for a destination folder using platform-specific APIs.
  void HandleSelectDownloadLocation(const ListValue* args);

  // Callback for the "promptForDownloadAction" message.  This will set
  // the ask for save location pref accordingly.
  void HandlePromptForDownload(const ListValue* args);

  // Callback for the "autoOpenFileTypesResetToDefault" message.  This will
  // remove all auto-open file-type settings.
  void HandleAutoOpenButton(const ListValue* args);

  // Callback for the "metricsReportingCheckboxAction" message.  This is called
  // if the user toggles the metrics reporting checkbox.
  void HandleMetricsReportingCheckbox(const ListValue* args);

  // Callback for the "defaultFontSizeAction" message.  This is called if the
  // user changes the default font size.  |args| is an array that contains
  // one item, the font size as a numeric value.
  void HandleDefaultFontSize(const ListValue* args);

  // Callback for the "Check for server certificate revocation" checkbox. This
  // is called if the user toggles the "Check for server certificate revocation"
  // checkbox.
  void HandleCheckRevocationCheckbox(const ListValue* args);

  // Callback for the "Use SSL 3.0" checkbox. This is called if the user toggles
  // the "Use SSL 3.0" checkbox.
  void HandleUseSSL3Checkbox(const ListValue* args);

  // Callback for the "Use TLS 1.0" checkbox. This is called if the user toggles
  // the "Use TLS 1.0" checkbox.
  void HandleUseTLS1Checkbox(const ListValue* args);

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

#if !defined(OS_CHROMEOS)
  // Callback for the Sign in to Cloud Print button.  This will start
  // the authentication process.
  void ShowCloudPrintSetupDialog(const ListValue* args);

  // Callback for the Disable Cloud Print button.  This will sign out
  // of cloud print.
  void HandleDisableCloudPrintProxy(const ListValue* args);

  // Callback for the Cloud Print manage button.  This will open a new
  // tab pointed at the management URL.
  void ShowCloudPrintManagePage(const ListValue* args);

  // Pings the service to send us it's current notion of the enabled state.
  void RefreshCloudPrintStatusFromService();

  // Setup the enabled or disabled state of the cloud print proxy
  // management UI.
  void SetupCloudPrintProxySection();

  // Remove cloud print proxy section if cloud print proxy management UI is
  // disabled.
  void RemoveCloudPrintProxySection();

#endif

#if defined(ENABLE_REMOTING) && !defined(OS_CHROMEOS)
  // Removes remoting section. Called if remoting is not enabled.
  void RemoveRemotingSection();

  // Callback for Setup Remoting button.
  void ShowRemotingSetupDialog(const ListValue* args);

  // Disable Remoting.
  void DisableRemoting(const ListValue* args);
#endif

  // Setup the checked state for the metrics reporting checkbox.
  void SetupMetricsReportingCheckbox();

  // Setup the visibility for the metrics reporting setting.
  void SetupMetricsReportingSettingVisibility();

  void SetupFontSizeLabel();

  // Setup the download path based on user preferences.
  void SetupDownloadLocationPath();

  // Setup the pref whether to prompt for download location every time.
  void SetupPromptForDownload();

  // Setup the enabled state of the reset button.
  void SetupAutoOpenFileTypesDisabledAttribute();

  // Setup the proxy settings section UI.
  void SetupProxySettingsSection();

  // Setup the checked state for SSL related checkboxes.
  void SetupSSLConfigSettings();

  scoped_refptr<SelectFileDialog> select_folder_dialog_;

#if !defined(OS_CHROMEOS)
  BooleanPrefMember enable_metrics_recording_;
  StringPrefMember cloud_print_proxy_email_;
  BooleanPrefMember cloud_print_proxy_enabled_;
  bool cloud_print_proxy_ui_enabled_;
  scoped_ptr<CloudPrintSetupHandler> cloud_print_setup_handler_;
#endif

  // SSLConfigService prefs.
  BooleanPrefMember rev_checking_enabled_;
  BooleanPrefMember ssl3_enabled_;
  BooleanPrefMember tls1_enabled_;

#if defined(ENABLE_REMOTING) && !defined(OS_CHROMEOS)
  remoting::RemotingOptionsHandler remoting_options_handler_;
#endif

  FilePathPrefMember default_download_location_;
  BooleanPrefMember ask_for_save_location_;
  StringPrefMember auto_open_files_;
  IntegerPrefMember default_font_size_;
  scoped_ptr<PrefSetObserver> proxy_prefs_;
  scoped_ptr<OptionsManagedBannerHandler> banner_handler_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedOptionsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_ADVANCED_OPTIONS_HANDLER_H_

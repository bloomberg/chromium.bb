// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/shell_dialogs.h"

#if defined(OS_MACOSX)
#include "chrome/browser/dom_ui/advanced_options_utils_mac.h"
#endif

// Chrome advanced options page UI handler.
class AdvancedOptionsHandler
    : public OptionsPageUIHandler,
      public SelectFileDialog::Listener {
 public:
  AdvancedOptionsHandler();
  virtual ~AdvancedOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const FilePath& path, int index, void* params);

 private:
  // Callback for the "selectDownloadLocation" message.  This will prompt
  // the user for a destination folder using platform-specific APIs.
  void HandleSelectDownloadLocation(const Value* value);

  // Callback for the "autoOpenFileTypesResetToDefault" message.  This will
  // remove all auto-open file-type settings.
  void HandleAutoOpenButton(const Value* value);

#if defined(OS_WIN)
  // Callback for the "Check SSL Revocation" checkbox.  This is needed so we
  // can support manual handling on Windows.
  void HandleCheckRevocationCheckbox(const Value* value);

  // Callback for the "Use SSL2" checkbox.  This is needed so we can support
  // manual handling on Windows.
  void HandleUseSSL2Checkbox(const Value* value);
#endif

  // Setup the download path based on user preferences.
  void SetupDownloadLocationPath();

  // Setup the enabled state of the reset button.
  void SetupAutoOpenFileTypesDisabledAttribute();

#if defined(OS_WIN)
  // Setup the checked state SSL related checkboxes.
  void SetupSSLConfigSettings();
#endif

  // Callback for the "showNetworkProxySettings" message. This will invoke
  // an appropriate dialog for configuring proxy settings.
  void ShowNetworkProxySettings(const Value* value);

  // Callback for the "showManageSSLCertificates" message. This will invoke
  // an appropriate certificate management action based on the platform.
  void ShowManageSSLCertificates(const Value* value);

  scoped_refptr<SelectFileDialog> select_folder_dialog_;
  FilePathPrefMember default_download_location_;
  StringPrefMember auto_open_files_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_HANDLER_H_

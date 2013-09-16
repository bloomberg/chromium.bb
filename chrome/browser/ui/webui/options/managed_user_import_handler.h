// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_IMPORT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_IMPORT_HANDLER_H_

#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace options {

// Handler for the 'import existing managed user' dialog.
class ManagedUserImportHandler : public OptionsPageUIHandler {
 public:
  ManagedUserImportHandler();
  virtual ~ManagedUserImportHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Callback for the "requestExistingManagedUsers" message.
  // Sends an array of managed users to WebUI. Each entry is of the form:
  //   managedProfile = {
  //     id: "Managed User ID",
  //     name: "Managed User Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     onCurrentDevice: true or false,
  //     needAvatar: true or false
  //   }
  // The array holds all existing managed users attached to the
  // custodian's profile who initiated the request except for
  // those managed users that already exist on this machine.
  void RequestExistingManagedUsers(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(ManagedUserImportHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_IMPORT_HANDLER_H_

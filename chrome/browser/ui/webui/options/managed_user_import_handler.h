// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_IMPORT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_IMPORT_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

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
  virtual void InitializeHandler() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  // Callback for the "requestManagedUserImportUpdate" message.
  // Checks the sign-in status of the custodian and accordingly
  // sends an update to the WebUI. The update can be to show/hide
  // an error bubble and update/clear the managed user list.
  void RequestManagedUserImportUpdate(const base::ListValue* args);

  // Sends an array of managed users to WebUI. Each entry is of the form:
  //   managedProfile = {
  //     id: "Managed User ID",
  //     name: "Managed User Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     onCurrentDevice: true or false,
  //     needAvatar: true or false
  //   }
  // The array holds all existing managed users attached to the
  // custodian's profile which initiated the request.
  void SendExistingManagedUsers(const base::DictionaryValue* dict);

  // Sends messages to the JS side to clear managed users and show an error
  // bubble.
  void ClearManagedUsersAndShowError();

  bool IsAccountConnected() const;
  bool HasAuthError() const;

  base::WeakPtrFactory<ManagedUserImportHandler> weak_ptr_factory_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserImportHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_IMPORT_HANDLER_H_

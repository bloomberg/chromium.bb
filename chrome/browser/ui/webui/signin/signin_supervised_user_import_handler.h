// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_SUPERVISED_USER_IMPORT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_SUPERVISED_USER_IMPORT_HANDLER_H_

#include <string>

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}

// Handler for the 'import existing supervised user' dialog.
class SigninSupervisedUserImportHandler : public content::WebUIMessageHandler {
 public:
  SigninSupervisedUserImportHandler();
  ~SigninSupervisedUserImportHandler() override;

  void GetLocalizedValues(base::DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SigninSupervisedUserImportHandlerTest,
                           NotAuthenticated);
  FRIEND_TEST_ALL_PREFIXES(SigninSupervisedUserImportHandlerTest, AuthError);
  FRIEND_TEST_ALL_PREFIXES(SigninSupervisedUserImportHandlerTest,
                           CustodianIsSupervised);
  FRIEND_TEST_ALL_PREFIXES(SigninSupervisedUserImportHandlerTest,
                           SendExistingSupervisedUsers);
  // Assigns a new |webui_callback_id_|. Ensures that previous in-flight request
  // has been fulfilled.
  void AssignWebUICallbackId(const base::ListValue* args);

  // Callback for the "openUrlInLastActiveProfileBrowser" message. Opens the
  // given url in a new background tab in the browser owned by the last active
  // profile. Hyperlinks don't work in the user manager since the system profile
  // browser is not tabbed.
  void OpenUrlInLastActiveProfileBrowser(const base::ListValue* args);

  // Callback for the "getExistingSupervisedUsers" message.
  // Checks the sign-in status of the custodian and accordingly
  // sends an update to the WebUI.
  void GetExistingSupervisedUsers(const base::ListValue* args);

  void LoadCustodianProfileCallback(Profile* custodian_profile,
                                    Profile::CreateStatus status);

  // Reject the WebUI callback with an error message.
  void RejectCallback(const base::string16& error);

  base::string16 GetLocalErorrMessage() const;

  base::string16 GetAuthErorrMessage() const;

  // Sends an array of supervised users to WebUI. Each entry is of the form:
  //   supervisedProfile = {
  //     id: "Supervised User ID",
  //     name: "Supervised User Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     onCurrentDevice: true or false,
  //   }
  // The array holds all existing supervised users attached to the
  // custodian's profile which initiated the request.
  void SendExistingSupervisedUsers(Profile* profile,
                                   const base::DictionaryValue* dict);

  bool IsAccountConnected(Profile* profile) const;
  bool HasAuthError(Profile* profile) const;

  // The WebUI callback ID of the last in-flight async request. There is always
  // only one in-flight such request.
  std::string webui_callback_id_;

  base::WeakPtrFactory<SigninSupervisedUserImportHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninSupervisedUserImportHandler);
};

#endif // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_SUPERVISED_USER_IMPORT_HANDLER_H_

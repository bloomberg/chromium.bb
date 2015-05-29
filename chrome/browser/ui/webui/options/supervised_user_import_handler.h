// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_SUPERVISED_USER_IMPORT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_SUPERVISED_USER_IMPORT_HANDLER_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/signin/core/browser/signin_error_controller.h"

namespace base {
class DictionaryValue;
class ListValue;
}

class ProfileInfoCache;
class SupervisedUserSyncService;

namespace options {

// Handler for the 'import existing supervised user' dialog.
class SupervisedUserImportHandler : public OptionsPageUIHandler,
                                    public ProfileInfoCacheObserver,
                                    public SupervisedUserSyncServiceObserver,
                                    public SigninErrorController::Observer {
 public:
  typedef base::CallbackList<void(const std::string&, const std::string&)>
      CallbackList;

  SupervisedUserImportHandler();
  ~SupervisedUserImportHandler() override;

 private:
  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // ProfileInfoCacheObserver implementation.
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileIsOmittedChanged(const base::FilePath& profile_path) override;

  // SupervisedUserSyncServiceObserver implementation.
  void OnSupervisedUserAcknowledged(
      const std::string& supervised_user_id) override {}
  void OnSupervisedUsersSyncingStopped() override {}
  void OnSupervisedUsersChanged() override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // Clears the cached list of supervised users and fetches the new list of
  // supervised users.
  void FetchSupervisedUsers();

  // Callback for the "requestSupervisedUserImportUpdate" message.
  // Checks the sign-in status of the custodian and accordingly
  // sends an update to the WebUI. The update can be to show/hide
  // an error bubble and update/clear the supervised user list.
  void RequestSupervisedUserImportUpdate(const base::ListValue* args);

  // Sends an array of supervised users to WebUI. Each entry is of the form:
  //   supervisedProfile = {
  //     id: "Supervised User ID",
  //     name: "Supervised User Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     onCurrentDevice: true or false,
  //     needAvatar: true or false
  //   }
  // The array holds all existing supervised users attached to the
  // custodian's profile which initiated the request.
  void SendExistingSupervisedUsers(const base::DictionaryValue* dict);

  // Sends messages to the JS side to clear supervised users and show an error
  // bubble.
  void ClearSupervisedUsersAndShowError();

  bool IsAccountConnected() const;
  bool HasAuthError() const;

  // Called when a supervised user shared setting is changed. If the avatar was
  // changed, FetchSupervisedUsers() is called.
  void OnSharedSettingChanged(const std::string& supervised_user_id,
                              const std::string& key);

  scoped_ptr<CallbackList::Subscription> subscription_;

  ScopedObserver<ProfileInfoCache, SupervisedUserImportHandler>
      profile_observer_;
  ScopedObserver<SigninErrorController, SupervisedUserImportHandler>
      signin_error_observer_;
  ScopedObserver<SupervisedUserSyncService, SupervisedUserImportHandler>
      supervised_user_sync_service_observer_;

  bool removed_profile_is_supervised_;

  base::WeakPtrFactory<SupervisedUserImportHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserImportHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_SUPERVISED_USER_IMPORT_HANDLER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGE_PROFILE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGE_PROFILE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_driver/sync_service_observer.h"

namespace base {
class StringValue;
}

namespace options {

// Chrome personal stuff profiles manage overlay UI handler.
class ManageProfileHandler : public OptionsPageUIHandler,
                             public ProfileInfoCacheObserver,
                             public sync_driver::SyncServiceObserver {
 public:
  ManageProfileHandler();
  ~ManageProfileHandler() override;

  // OptionsPageUIHandler:
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;
  void Uninitialize() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // ProfileInfoCacheObserver:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

  // sync_driver::SyncServiceObserver:
  void OnStateChanged() override;

 private:
  // This function creates signed in user specific strings in loadTimeData.
  void GenerateSignedinUserSpecificStrings(base::DictionaryValue* dictionary);

  // Callback for the "requestDefaultProfileIcons" message.
  // Sends the array of default profile icon URLs and profile names to WebUI.
  // First item of |args| is the dialog mode, i.e. "create" or "manage".
  void RequestDefaultProfileIcons(const base::ListValue* args);

  // Callback for the "requestNewProfileDefaults" message.
  // Sends an object to WebUI of the form:
  //   { "name": profileName, "iconURL": iconURL }
  void RequestNewProfileDefaults(const base::ListValue* args);

  // Send all profile icons and their default names to the overlay.
  // |mode| is the dialog mode, i.e. "create" or "manage".
  void SendProfileIconsAndNames(const base::StringValue& mode);

  // Sends an object to WebUI of the form:
  //   profileNames = {
  //     "Profile Name 1": true,
  //     "Profile Name 2": true,
  //     ...
  //   };
  // This is used to detect duplicate profile names.
  void SendExistingProfileNames();

  // Show disconnect managed profile dialog after generating domain and user
  // specific strings.
  void ShowDisconnectManagedProfileDialog(const base::ListValue* args);

  // Callback for the "setProfileIconAndName" message. Sets the name and icon
  // of a given profile.
  // |args| is of the form: [
  //   /*string*/ profileFilePath,
  //   /*string*/ newProfileIconURL
  //   /*string*/ newProfileName,
  // ]
  void SetProfileIconAndName(const base::ListValue* args);

#if defined(ENABLE_SETTINGS_APP)
  // Callback for the "switchAppListProfile" message. Asks the
  // app_list_controller to change the profile registered for the AppList.
  // |args| is of the form: [ {string} profileFilePath ]
  void SwitchAppListProfile(const base::ListValue* args);
#endif

  // Callback for the 'profileIconSelectionChanged' message. Used to update the
  // name in the manager profile dialog based on the selected icon.
  void ProfileIconSelectionChanged(const base::ListValue* args);

  // Callback for the "requestHasProfileShortcuts" message, which is called
  // when editing an existing profile. Asks the profile shortcut manager whether
  // the profile has shortcuts and gets the result in |OnHasProfileShortcuts()|.
  // |args| is of the form: [ {string} profileFilePath ]
  void RequestHasProfileShortcuts(const base::ListValue* args);

  // Callback for the "RequestCreateProfileUpdate" message.
  // Sends the email address of the signed-in user, or an empty string if the
  // user is not signed in. Also sends information about whether supervised
  // users may be created.
  void RequestCreateProfileUpdate(const base::ListValue* args);

  // When the pref allowing supervised-user creation changes, sends the new
  // value to the UI.
  void OnCreateSupervisedUserPrefChange();

  // Callback invoked from the profile manager indicating whether the profile
  // being edited has any desktop shortcuts.
  void OnHasProfileShortcuts(bool has_shortcuts);

  // Callback for the "addProfileShortcut" message, which is called when editing
  // an existing profile and the user clicks the "Add desktop shortcut" button.
  // Adds a desktop shortcut for the profile.
  void AddProfileShortcut(const base::ListValue* args);

  // Callback for the "removeProfileShortcut" message, which is called when
  // editing an existing profile and the user clicks the "Remove desktop
  // shortcut" button. Removes the desktop shortcut for the profile.
  void RemoveProfileShortcut(const base::ListValue* args);

  // Callback for the "refreshGaiaPicture" message, which is called when the
  // user is editing an existing profile.
  void RefreshGaiaPicture(const base::ListValue* args);

  // URL for the current profile's GAIA picture.
  std::string gaia_picture_url_;

  // Used to observe the preference that allows creating supervised users, which
  // can be changed by policy.
  PrefChangeRegistrar pref_change_registrar_;

  // For generating weak pointers to itself for callbacks.
  base::WeakPtrFactory<ManageProfileHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManageProfileHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGE_PROFILE_HANDLER_H_

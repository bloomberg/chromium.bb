// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/credential_manager_dispatcher.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/sync/browser/sync_credentials_filter.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/rect.h"

class Profile;

namespace autofill {
class PasswordGenerationPopupObserver;
class PasswordGenerationPopupControllerImpl;
}

namespace content {
class WebContents;
}

namespace password_manager {
struct CredentialInfo;
class PasswordGenerationManager;
class PasswordManagerDriver;
}

// ChromePasswordManagerClient implements the PasswordManagerClient interface.
class ChromePasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromePasswordManagerClient> {
 public:
  ~ChromePasswordManagerClient() override;

  // PasswordManagerClient implementation.
  bool IsAutomaticPasswordSavingEnabled() const override;
  bool IsSavingAndFillingEnabledForCurrentPage() const override;
  bool IsFillingEnabledForCurrentPage() const override;
  bool PromptUserToSaveOrUpdatePassword(
      scoped_ptr<password_manager::PasswordFormManager> form_to_save,
      password_manager::CredentialSourceType type,
      bool update_password) override;
  bool PromptUserToChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_forms,
      ScopedVector<autofill::PasswordForm> federated_forms,
      const GURL& origin,
      base::Callback<void(const password_manager::CredentialInfo&)> callback)
      override;
  void ForceSavePassword() override;
  void GeneratePassword() override;
  void NotifyUserAutoSignin(
      ScopedVector<autofill::PasswordForm> local_forms) override;
  void NotifyUserAutoSigninBlockedOnFirstRun(
      scoped_ptr<autofill::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form) override;
  void AutomaticPasswordSave(scoped_ptr<password_manager::PasswordFormManager>
                                 saved_form_manager) override;
  void PasswordWasAutofilled(const autofill::PasswordFormMap& best_matches,
                             const GURL& origin) const override;
  PrefService* GetPrefs() override;
  password_manager::PasswordStore* GetPasswordStore() const override;
  password_manager::PasswordSyncState GetPasswordSyncState() const override;
  bool WasLastNavigationHTTPError() const override;
  bool DidLastPageLoadEncounterSSLErrors() const override;
  bool IsOffTheRecord() const override;
  const password_manager::PasswordManager* GetPasswordManager() const override;
  autofill::AutofillManager* GetAutofillManagerForMainFrame() override;
  const GURL& GetMainFrameURL() const override;
  bool IsUpdatePasswordUIEnabled() const override;
  const GURL& GetLastCommittedEntryURL() const override;
  const password_manager::CredentialsFilter* GetStoreResultFilter()
      const override;
  const password_manager::LogManager* GetLogManager() const override;

  // Hides any visible generation UI.
  void HidePasswordGenerationPopup();

  static void CreateForWebContentsWithAutofillClient(
      content::WebContents* contents,
      autofill::AutofillClient* autofill_client);

  // Observer for PasswordGenerationPopup events. Used for testing.
  void SetTestObserver(autofill::PasswordGenerationPopupObserver* observer);

  // Returns true if the bubble UI is enabled, and false if we're still using
  // the sad old Infobar UI.
  static bool IsTheHotNewBubbleUIEnabled();

  // Returns true if the password manager should be enabled during sync signin.
  static bool EnabledForSyncSignin();

 protected:
  // Callable for tests.
  ChromePasswordManagerClient(content::WebContents* web_contents,
                              autofill::AutofillClient* autofill_client);

 private:
  friend class content::WebContentsUserData<ChromePasswordManagerClient>;

  // content::WebContentsObserver overrides.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Given |bounds| in the renderers coordinate system, return the same bounds
  // in the screens coordinate system.
  gfx::RectF GetBoundsInScreenSpace(const gfx::RectF& bounds);

  // Checks if the current page fulfils the conditions for the password manager
  // to be active on it, for example Sync credentials are not saved or auto
  // filled.
  bool IsPasswordManagementEnabledForCurrentPage() const;

  // Causes the password generation UI to be shown for the specified form.
  // The popup will be anchored at |element_bounds|. The generated password
  // will be no longer than |max_length|.
  void ShowPasswordGenerationPopup(content::RenderFrameHost* render_frame_host,
                                   const gfx::RectF& bounds,
                                   int max_length,
                                   const autofill::PasswordForm& form);

  // Causes the password editing UI to be shown anchored at |element_bounds|.
  void ShowPasswordEditingPopup(content::RenderFrameHost* render_frame_host,
                                const gfx::RectF& bounds,
                                const autofill::PasswordForm& form);

  // Shows the dialog where the user can accept or decline the global autosignin
  // setting as a first run experience. The dialog won't appear in Incognito or
  // when the autosign-in is off.
  void PromptUserToEnableAutosigninIfNecessary();

  // Notify the PasswordManager that generation is available for |form|. Used
  // for UMA stats.
  void GenerationAvailableForForm(const autofill::PasswordForm& form);

  // Called as a response to PromptUserToChooseCredentials.
  void OnCredentialsChosen(
      base::Callback<void(const password_manager::CredentialInfo&)> callback,
      const password_manager::CredentialInfo& credential);

  Profile* const profile_;

  password_manager::PasswordManager password_manager_;

  password_manager::ContentPasswordManagerDriverFactory* driver_factory_;

  password_manager::CredentialManagerDispatcher
      credential_manager_dispatcher_;

  // Observer for password generation popup.
  autofill::PasswordGenerationPopupObserver* observer_;

  // Controls the popup
  base::WeakPtr<
    autofill::PasswordGenerationPopupControllerImpl> popup_controller_;

  // Set to false to disable password saving (will no longer ask if you
  // want to save passwords and also won't fill the passwords).
  BooleanPrefMember saving_and_filling_passwords_enabled_;

  const password_manager::SyncCredentialsFilter credentials_filter_;

  scoped_ptr<password_manager::LogManager> log_manager_;

  // Set during 'NotifyUserAutoSigninBlockedOnFirstRun' in order to store the
  // form for potential use during 'NotifySuccessfulLoginWithExistingPassword'.
  scoped_ptr<autofill::PasswordForm> form_blocked_on_first_run_;

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordManagerClient);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

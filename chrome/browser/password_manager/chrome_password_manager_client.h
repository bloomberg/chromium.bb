// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/rect.h"

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
class PasswordManager;
}

// ChromePasswordManagerClient implements the PasswordManagerClient interface.
class ChromePasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromePasswordManagerClient> {
 public:
  virtual ~ChromePasswordManagerClient();

  // PasswordManagerClient implementation.
  virtual bool IsAutomaticPasswordSavingEnabled() const OVERRIDE;
  virtual bool IsPasswordManagerEnabledForCurrentPage() const OVERRIDE;
  virtual bool ShouldFilterAutofillResult(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual bool IsSyncAccountCredential(
      const std::string& username, const std::string& origin) const OVERRIDE;
  virtual void AutofillResultsComputed() OVERRIDE;
  virtual bool PromptUserToSavePassword(
      scoped_ptr<password_manager::PasswordFormManager> form_to_save) OVERRIDE;
  virtual void AutomaticPasswordSave(
      scoped_ptr<password_manager::PasswordFormManager> saved_form_manager)
      OVERRIDE;
  virtual void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches) const OVERRIDE;
  virtual void PasswordAutofillWasBlocked(
      const autofill::PasswordFormMap& best_matches) const OVERRIDE;
  virtual void AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual password_manager::PasswordStore* GetPasswordStore() OVERRIDE;
  virtual password_manager::PasswordManagerDriver* GetDriver() OVERRIDE;
  virtual base::FieldTrial::Probability GetProbabilityForExperiment(
      const std::string& experiment_name) OVERRIDE;
  virtual bool IsPasswordSyncEnabled() OVERRIDE;
  virtual void OnLogRouterAvailabilityChanged(bool router_can_be_used) OVERRIDE;
  virtual void LogSavePasswordProgress(const std::string& text) OVERRIDE;
  virtual bool IsLoggingActive() const OVERRIDE;
  virtual void OnNotifyFailedSignIn(
      int request_id,
      const password_manager::CredentialInfo&) OVERRIDE;
  virtual void OnNotifySignedIn(
      int request_id,
      const password_manager::CredentialInfo&) OVERRIDE;
  virtual void OnNotifySignedOut(int request_id) OVERRIDE;
  virtual void OnRequestCredential(
      int request_id,
      bool zero_click_only,
      const std::vector<GURL>& federations) OVERRIDE;

  // Hides any visible generation UI.
  void HidePasswordGenerationPopup();

  static void CreateForWebContentsWithAutofillClient(
      content::WebContents* contents,
      autofill::AutofillClient* autofill_client);

  // Convenience method to allow //chrome code easy access to a PasswordManager
  // from a WebContents instance.
  static password_manager::PasswordManager* GetManagerFromWebContents(
      content::WebContents* contents);

  // Convenience method to allow //chrome code easy access to a
  // PasswordGenerationManager from a WebContents instance.
  static password_manager::PasswordGenerationManager*
      GetGenerationManagerFromWebContents(content::WebContents* contents);

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
  enum AutofillForSyncCredentialsState {
    ALLOW_SYNC_CREDENTIALS,
    DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH,
    DISALLOW_SYNC_CREDENTIALS,
  };

  friend class content::WebContentsUserData<ChromePasswordManagerClient>;

  // content::WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Callback method to be triggered when authentication is successful for a
  // given password authentication request.  If authentication is disabled or
  // not supported, this will be triggered directly.
  void CommitFillPasswordForm(autofill::PasswordFormFillData* fill_data);

  // Given |bounds| in the renderers coordinate system, return the same bounds
  // in the screens coordinate system.
  gfx::RectF GetBoundsInScreenSpace(const gfx::RectF& bounds);

  // Causes the password generation UI to be shown for the specified form.
  // The popup will be anchored at |element_bounds|. The generated password
  // will be no longer than |max_length|.
  void ShowPasswordGenerationPopup(const gfx::RectF& bounds,
                                   int max_length,
                                   const autofill::PasswordForm& form);

  // Causes the password editing UI to be shown anchored at |element_bounds|.
  void ShowPasswordEditingPopup(
      const gfx::RectF& bounds, const autofill::PasswordForm& form);

  // Sends a message to the renderer with the current value of
  // |can_use_log_router_|.
  void NotifyRendererOfLoggingAvailability();

  // Returns true if the last loaded page was for transactional re-auth on a
  // Google property.
  bool LastLoadWasTransactionalReauthPage() const;

  // Returns true if |url| is the reauth page for accessing the password
  // website.
  bool IsURLPasswordWebsiteReauth(const GURL& url) const;

  // Sets |autofill_state_| based on experiment and flag values.
  void SetUpAutofillSyncState();

  Profile* const profile_;

  password_manager::ContentPasswordManagerDriver driver_;

  // Observer for password generation popup.
  autofill::PasswordGenerationPopupObserver* observer_;

  // Controls the popup
  base::WeakPtr<
    autofill::PasswordGenerationPopupControllerImpl> popup_controller_;

  // True if |this| is registered with some LogRouter which can accept logs.
  bool can_use_log_router_;

  // How to handle the sync credential in ShouldFilterAutofillResult().
  AutofillForSyncCredentialsState autofill_sync_state_;

  // If the sync credential was filtered during autofill. Used for statistics
  // reporting.
  bool sync_credential_was_filtered_;

  // Allows authentication callbacks to be destroyed when this client is gone.
  base::WeakPtrFactory<ChromePasswordManagerClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordManagerClient);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

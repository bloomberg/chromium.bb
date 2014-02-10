// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/password_manager/content_password_manager_driver.h"
#include "chrome/browser/password_manager/password_manager_client.h"
#include "content/public/browser/web_contents_user_data.h"

class PasswordGenerationManager;
class PasswordManager;

namespace content {
class WebContents;
}

// ChromePasswordManagerClient implements the PasswordManagerClient interface.
class ChromePasswordManagerClient
    : public PasswordManagerClient,
      public content::WebContentsUserData<ChromePasswordManagerClient> {
 public:
  virtual ~ChromePasswordManagerClient();

  // PasswordManagerClient implementation.
  virtual void PromptUserToSavePassword(PasswordFormManager* form_to_save)
      OVERRIDE;
  virtual void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches) const OVERRIDE;
  virtual Profile* GetProfile() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual PasswordManagerDriver* GetDriver() OVERRIDE;
  virtual base::FieldTrial::Probability GetProbabilityForExperiment(
      const std::string& experiment_name) OVERRIDE;
  virtual bool IsPasswordSyncEnabled() OVERRIDE;

  // Convenience method to allow //chrome code easy access to a PasswordManager
  // from a WebContents instance.
  static PasswordManager* GetManagerFromWebContents(
      content::WebContents* contents);

  // Convenience method to allow //chrome code easy access to a
  // PasswordGenerationManager
  // from a WebContents instance.
  static PasswordGenerationManager* GetGenerationManagerFromWebContents(
      content::WebContents* contents);

 private:
  explicit ChromePasswordManagerClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromePasswordManagerClient>;

  content::WebContents* web_contents_;
  ContentPasswordManagerDriver driver_;

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordManagerClient);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
struct FormData;
class FormStructure;
class PasswordGenerator;
struct PasswordForm;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Per-tab manager for password generation. Will enable this feature only if
//
// -  Password manager is enabled
// -  Password sync is enabled
// -  Password generation pref is enabled
//
// NOTE: At the moment, the creation of the renderer PasswordGenerationManager
// is controlled by a switch (--enable-password-generation) so this feature will
// not be enabled regardless of the above criteria without the switch being
// present.
//
// This class is used to determine what forms we should offer to generate
// passwords for and manages the popup which is created if the user chooses to
// generate a password.
class PasswordGenerationManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PasswordGenerationManager> {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  virtual ~PasswordGenerationManager();

  // Detect account creation forms from forms with autofill type annotated.
  // Will send a message to the renderer if we find a correctly annotated form
  // and the feature is enabled.
  void DetectAccountCreationForms(
      const std::vector<autofill::FormStructure*>& forms);

 protected:
  explicit PasswordGenerationManager(content::WebContents* contents);

 private:
  friend class content::WebContentsUserData<PasswordGenerationManager>;
  friend class PasswordGenerationManagerTest;

  // WebContentsObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Determines current state of password generation
  bool IsGenerationEnabled() const;

  // Sends a message to the renderer specifying form(s) that we should enable
  // password generation on. This is a separate function to aid in testing.
  virtual void SendAccountCreationFormsToRenderer(
      content::RenderViewHost* host,
      const std::vector<autofill::FormData>& forms);

  // Causes the password generation bubble UI to be shown for the specified
  // form. The popup will be anchored at |icon_bounds|. The generated
  // password will be no longer than |max_length|.
  void OnShowPasswordGenerationPopup(const gfx::Rect& icon_bounds,
                                     int max_length,
                                     const autofill::PasswordForm& form);

  // Controls how passwords are generated.
  scoped_ptr<autofill::PasswordGenerator> password_generator_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManager);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class PasswordGenerator;
}

namespace content {
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
// When enabled we will send a message enabling this feature in the renderer,
// which will show an icon next to password fields which we think are associated
// with account creation. This class also manages the popup which is created
// if the user chooses to generate a password.
class PasswordGenerationManager
    : public ProfileSyncServiceObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<PasswordGenerationManager> {
 public:
  static void CreateForWebContents(content::WebContents* contents);
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);
  virtual ~PasswordGenerationManager();

 protected:
  explicit PasswordGenerationManager(content::WebContents* contents);

 private:
  friend class content::WebContentsUserData<PasswordGenerationManager>;
  friend class PasswordGenerationManagerTest;

  // WebContentsObserver:
  virtual void RenderViewCreated(content::RenderViewHost* host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents* contents) OVERRIDE;

  // ProfileSyncServiceObserver:
  virtual void OnStateChanged() OVERRIDE;

  // Add ourselves as an observer to the sync service to be informed of changes
  // to the password sync state.
  void RegisterWithSyncService();

  // Start watching for changes to the password generation enabled pref.
  void SetUpPrefChangeRegistrar();
  void OnPrefStateChanged();

  // Determines current state of password generation and sends this information
  // to the renderer if it is different from |enabled_| or if |new_renderer|
  // is true.
  void UpdateState(content::RenderViewHost* host, bool new_renderer);

  // Sends a message to the renderer enabling or disabling this feature. This
  // is a separate function to aid in testing.
  virtual void SendStateToRenderer(content::RenderViewHost* host, bool enabled);

  // Causes the password generation bubble UI to be shown for the specified
  // form. The popup will be anchored at |icon_bounds|. The generated
  // password will be no longer than |max_length|.
  void OnShowPasswordGenerationPopup(const gfx::Rect& icon_bounds,
                                     int max_length,
                                     const content::PasswordForm& form);

  // Whether password generation is enabled.
  bool enabled_;

  // Listens for changes to the state of the password generation pref.
  PrefChangeRegistrar registrar_;

  // For vending a weak_ptr for |registrar_|.
  base::WeakPtrFactory<PasswordGenerationManager> weak_factory_;

  // Controls how passwords are generated.
  scoped_ptr<autofill::PasswordGenerator> password_generator_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManager);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_

#include "base/macros.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefChangeRegistrar;
class Profile;

namespace chromeos {

// MagnificationManager controls the full screen magnifier from chrome-browser
// side (not ash side).
//
// MagnificationManager does:
//   - Watch logged-in. Changes the behavior between the login screen and user
//     desktop.
//   - Watch change of the pref. When the pref changes, the setting of the
//     magnifier will interlock with it.
class MagnificationManager
    : public content::NotificationObserver,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  // Creates an instance of MagnificationManager. This should be called once.
  static void Initialize();

  // Deletes the existing instance of MagnificationManager.
  static void Shutdown();

  // Returns the existing instance. If there is no instance, returns NULL.
  static MagnificationManager* Get();

  // Returns if the screen magnifier is enabled.
  bool IsMagnifierEnabled() const;

  // Enables the screen magnifier.
  void SetMagnifierEnabled(bool enabled);

  // Saves the magnifier scale to the pref.
  void SaveScreenMagnifierScale(double scale);

  // Loads the magnifier scale from the pref.
  double GetSavedScreenMagnifierScale() const;

  void SetProfileForTest(Profile* profile);

 private:
  MagnificationManager();
  ~MagnificationManager() override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void ActiveUserChanged(const user_manager::User* active_user) override;

  void SetProfile(Profile* profile);

  void SetMagnifierEnabledInternal(bool enabled);
  void SetMagnifierKeepFocusCenteredInternal(bool keep_focus_centered);
  void SetMagnifierScaleInternal(double scale);
  void UpdateMagnifierFromPrefs();

  void MonitorFocusInPageChange();

  Profile* profile_ = nullptr;

  bool enabled_ = false;
  bool keep_focus_centered_ = false;
  double scale_ = 0.0;
  bool observing_focus_change_in_page_ = false;

  content::NotificationRegistrar registrar_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<user_manager::ScopedUserSessionStateObserver>
      session_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(MagnificationManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_

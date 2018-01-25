// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen_view.h"

class Profile;

namespace chromeos {

class BaseScreenDelegate;

// This is Sync settings screen that is displayed as a part of user first
// sign-in flow.
class SyncConsentScreen : public BaseScreen {
 public:
  SyncConsentScreen(BaseScreenDelegate* base_screen_delegate,
                    SyncConsentScreenView* view);
  ~SyncConsentScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  // Modifies user sync preference on user action.
  void SetSyncAllValue(bool sync_all);

 private:
  SyncConsentScreenView* const view_;

  // Profile of the primary user (if screen is shown).
  Profile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SyncConsentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_

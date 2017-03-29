// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENCRYPTION_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENCRYPTION_MIGRATION_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen_view.h"

namespace chromeos {

class EncryptionMigrationScreen
    : public BaseScreen,
      public EncryptionMigrationScreenView::Delegate {
 public:
  EncryptionMigrationScreen(BaseScreenDelegate* base_screen_delegate,
                            EncryptionMigrationScreenView* view);
  ~EncryptionMigrationScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;

  // EncryptionMigrationScreenView::Delegate:
  void OnExit() override;
  void OnViewDestroyed(EncryptionMigrationScreenView* view) override;

 private:
  EncryptionMigrationScreenView* view_;

  DISALLOW_COPY_AND_ASSIGN(EncryptionMigrationScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENCRYPTION_MIGRATION_SCREEN_H_

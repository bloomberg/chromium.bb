// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// WebUI implementation of EncryptionMigrationScreenView
class EncryptionMigrationScreenHandler : public EncryptionMigrationScreenView,
                                         public BaseScreenHandler {
 public:
  EncryptionMigrationScreenHandler();
  ~EncryptionMigrationScreenHandler() override;

  // EncryptionMigrationScreenView:
  void Show() override;
  void Hide() override;
  void SetDelegate(Delegate* delegate) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  Delegate* delegate_ = nullptr;
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(EncryptionMigrationScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "components/autofill/core/browser/ui/local_card_migration_dialog_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class LocalCardMigrationDialog;

// This per-tab controller is lazily initialized and owns a
// LocalCardMigrationDialog. It's also responsible for reshowing the original
// dialog that the migration dialog interrupted.
class LocalCardMigrationDialogControllerImpl
    : public LocalCardMigrationDialogController,
      public content::WebContentsUserData<
          LocalCardMigrationDialogControllerImpl> {
 public:
  ~LocalCardMigrationDialogControllerImpl() override;

  void ShowDialog(LocalCardMigrationDialog* local_card_migration_Dialog,
                  base::OnceClosure user_accepted_migration_closure_);

  // LocalCardMigrationDialogController:
  LocalCardMigrationDialogState GetViewState() const override;
  void SetViewState(LocalCardMigrationDialogState view_state) override;
  std::vector<MigratableCreditCard>& GetCardList() override;
  void SetCardList(std::vector<MigratableCreditCard>& card_list) override;
  void OnDialogClosed() override;

 protected:
  explicit LocalCardMigrationDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      LocalCardMigrationDialogControllerImpl>;

  LocalCardMigrationDialog* local_card_migration_dialog_;

  LocalCardMigrationDialogState view_state_;

  std::vector<MigratableCreditCard> migratable_credit_cards_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
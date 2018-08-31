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

  void ShowDialog(std::unique_ptr<base::DictionaryValue> legal_message,
                  LocalCardMigrationDialog* local_card_migration_Dialog,
                  base::OnceClosure user_accepted_migration_closure_);

  // LocalCardMigrationDialogController:
  LocalCardMigrationDialogState GetViewState() const override;
  void SetViewState(LocalCardMigrationDialogState view_state) override;
  const std::vector<MigratableCreditCard>& GetCardList() const override;
  void SetCardList(std::vector<MigratableCreditCard>& card_list) override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  void OnCardSelected(int index) override;
  void OnDialogClosed() override;

 protected:
  explicit LocalCardMigrationDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      LocalCardMigrationDialogControllerImpl>;

  LocalCardMigrationDialog* local_card_migration_dialog_;

  LocalCardMigrationDialogState view_state_;

  LegalMessageLines legal_message_lines_;

  // TODO(crbug.com/867194): Currently we will not handle the case of local
  // cards added/deleted during migration. migratable_credit_cards_ are local
  // cards presented when the user accepts the intermediate bubble.
  std::vector<MigratableCreditCard> migratable_credit_cards_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
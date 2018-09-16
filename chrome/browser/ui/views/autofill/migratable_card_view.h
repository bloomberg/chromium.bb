// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_MIGRATABLE_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_MIGRATABLE_CARD_VIEW_H_

#include "base/macros.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "ui/views/view.h"

namespace views {
class ButtonListener;
class Checkbox;
class ImageButton;
class ImageView;
}  // namespace views

namespace autofill {

enum class LocalCardMigrationDialogState;
class MigratableCreditCard;

// A view composed of a checkbox or an image indicating migration results, card
// network image, card network, last four digits of card number and card
// expiration date. Used by LocalCardMigrationDialogView.
class MigratableCardView : public views::View {
 public:
  static const char kViewClassName[];

  MigratableCardView(const MigratableCreditCard& migratable_credit_card,
                     views::ButtonListener* listener,
                     int card_index);
  ~MigratableCardView() override;

  bool IsSelected();
  std::string GetGuid();
  void SetCheckboxEnabled(bool checkbox_enabled);
  void UpdateCardView(LocalCardMigrationDialogState dialog_state);

  // views::View
  const char* GetClassName() const override;

 private:
  void Init(const MigratableCreditCard& migratable_credit_card,
            views::ButtonListener* listener,
            int card_index);

  MigratableCreditCard migratable_credit_card_;

  views::Checkbox* checkbox_ = nullptr;

  views::ImageView* migration_succeeded_image_ = nullptr;
  views::ImageView* migration_failed_image_ = nullptr;

  views::ImageButton* delete_card_from_local_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MigratableCardView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_MIGRATABLE_CARD_VIEW_H_

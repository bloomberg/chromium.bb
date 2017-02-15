// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credentials_selection_view.h"

#include <stddef.h>

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

views::Label* GeneratePasswordLabel(const autofill::PasswordForm& form) {
  views::Label* label = new views::Label(form.password_value);
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label->SetObscured(true);
  return label;
}

}  // namespace

CredentialsSelectionView::CredentialsSelectionView(
    ManagePasswordsBubbleModel* manage_passwords_bubble_model)
    : password_forms_(&manage_passwords_bubble_model->local_credentials()),
      default_index_(0),
      is_default_best_match_(false),
      is_default_preferred_(false),
      action_reported_(false) {
  DCHECK(!password_forms_->empty());

  // Layout.
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // ColumnSet.
  int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::FIXED, 0, 0);
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::FIXED, 0, 0);
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);

  // The username combobox and password label.
  layout->StartRowWithPadding(0, column_set_id, 0,
                              views::kRelatedControlVerticalSpacing);
  GenerateUsernameCombobox(
      manage_passwords_bubble_model->pending_password().username_value);
  layout->AddView(combobox_.get());
  views::Label* label =
      GeneratePasswordLabel(manage_passwords_bubble_model->pending_password());
  layout->AddView(label);

  GetLayoutManager()->Layout(this);
}

CredentialsSelectionView::~CredentialsSelectionView() {
  ReportUserActionOnce(true, -1);
  // |combobox_| has a pointer to |combobox_model_|, so |combobox_| should be
  // deleted before deleting of |combobox_model_|. To ensure this, let's delete
  // it now.
  combobox_.reset();
}

const autofill::PasswordForm*
CredentialsSelectionView::GetSelectedCredentials() {
  DCHECK_EQ(password_forms_->size(),
            static_cast<size_t>(combobox_->model()->GetItemCount()));
  ReportUserActionOnce(false, combobox_->selected_index());
  return &password_forms_->at(combobox_->selected_index());
}

void CredentialsSelectionView::GenerateUsernameCombobox(
    const base::string16& best_matched_username) {
  std::vector<base::string16> usernames;
  size_t best_matched_username_index = password_forms_->size();
  size_t preferred_form_index = password_forms_->size();
  for (size_t index = 0; index < password_forms_->size(); ++index) {
    usernames.push_back(password_forms_->at(index).username_value);
    if (password_forms_->at(index).username_value == best_matched_username) {
      best_matched_username_index = index;
    }
    if (password_forms_->at(index).preferred) {
      preferred_form_index = index;
    }
  }

  combobox_model_.reset(new ui::SimpleComboboxModel(usernames));
  combobox_.reset(new views::Combobox(combobox_model_.get()));

  if (best_matched_username_index < password_forms_->size()) {
    is_default_best_match_ = true;
    default_index_ = best_matched_username_index;
    combobox_->SetSelectedIndex(best_matched_username_index);
  } else if (preferred_form_index < password_forms_->size()) {
    is_default_preferred_ = true;
    default_index_ = preferred_form_index;
    combobox_->SetSelectedIndex(preferred_form_index);
  }
}

void CredentialsSelectionView::ReportUserActionOnce(bool was_update_rejected,
                                                    int selected_index) {
  if (action_reported_)
    return;
  password_manager::metrics_util::MultiAccountUpdateBubbleUserAction action;
  if (was_update_rejected) {
    if (is_default_best_match_) {
      action = password_manager::metrics_util::
          DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_REJECTED_UPDATE;
    } else if (is_default_preferred_) {
      action = password_manager::metrics_util::
          DEFAULT_ACCOUNT_PREFERRED_USER_REJECTED_UPDATE;
    } else {
      action = password_manager::metrics_util::
          DEFAULT_ACCOUNT_FIRST_USER_REJECTED_UPDATE;
    }
  } else if (selected_index == default_index_) {
    if (is_default_best_match_) {
      action = password_manager::metrics_util::
          DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_NOT_CHANGED;
    } else if (is_default_preferred_) {
      action = password_manager::metrics_util::
          DEFAULT_ACCOUNT_PREFERRED_USER_NOT_CHANGED;
    } else {
      action = password_manager::metrics_util::
          DEFAULT_ACCOUNT_FIRST_USER_NOT_CHANGED;
    }
  } else {
    if (is_default_best_match_) {
      action = password_manager::metrics_util::
          DEFAULT_ACCOUNT_MATCHED_BY_PASSWORD_USER_CHANGED;
    } else if (is_default_preferred_) {
      action = password_manager::metrics_util::
          DEFAULT_ACCOUNT_PREFERRED_USER_CHANGED;
    } else {
      action =
          password_manager::metrics_util::DEFAULT_ACCOUNT_FIRST_USER_CHANGED;
    }
  }

  password_manager::metrics_util::LogMultiAccountUpdateBubbleUserAction(action);
  action_reported_ = true;
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credentials_selection_view.h"

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
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

views::Combobox* GenerateUsernameCombobox(
    const std::vector<const autofill::PasswordForm*>& forms,
    const base::string16& best_matched_username) {
  std::vector<base::string16> usernames;
  size_t best_matched_username_index = forms.size();
  size_t preferred_form_index = forms.size();
  for (size_t index = 0; index < forms.size(); ++index) {
    usernames.push_back(forms[index]->username_value);
    if (forms[index]->username_value == best_matched_username) {
      best_matched_username_index = index;
    }
    if (forms[index]->preferred) {
      preferred_form_index = index;
    }
  }

  views::Combobox* combobox =
      new views::Combobox(new ui::SimpleComboboxModel(usernames));

  if (best_matched_username_index < forms.size()) {
    combobox->SetSelectedIndex(best_matched_username_index);
  } else if (preferred_form_index < forms.size()) {
    combobox->SetSelectedIndex(preferred_form_index);
  }
  return combobox;
}

}  // namespace

CredentialsSelectionView::CredentialsSelectionView(
    ManagePasswordsBubbleModel* manage_passwords_bubble_model,
    const std::vector<const autofill::PasswordForm*>& password_forms,
    const base::string16& best_matched_username)
    : password_forms_(password_forms) {
  DCHECK(!password_forms.empty());

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
  combobox_ = GenerateUsernameCombobox(
      manage_passwords_bubble_model->local_credentials().get(),
      best_matched_username);
  layout->AddView(combobox_);
  views::Label* label =
      GeneratePasswordLabel(manage_passwords_bubble_model->pending_password());
  layout->AddView(label);

  GetLayoutManager()->Layout(this);
}

const autofill::PasswordForm*
CredentialsSelectionView::GetSelectedCredentials() {
  DCHECK_EQ(password_forms_.size(),
            static_cast<size_t>(combobox_->model()->GetItemCount()));
  return password_forms_[combobox_->selected_index()];
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_credential_item_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

enum ColumnSets {
  TWO_COLUMN_SET,
};

void BuildColumnSet(views::GridLayout* layout) {
  views::ColumnSet* column_set = layout->AddColumnSet(TWO_COLUMN_SET);

  // The credential or "Deleted!" field.
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::CENTER,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  // The delete button or "Undo!" field.
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
  column_set->AddColumn(views::GridLayout::TRAILING,
                        views::GridLayout::CENTER,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
}

}  // namespace

ManageCredentialItemView::ManageCredentialItemView(
    ManagePasswordsBubbleModel* model,
    const autofill::PasswordForm* password_form)
    : form_(*password_form),
      delete_button_(nullptr),
      undo_link_(nullptr),
      model_(model),
      form_deleted_(false) {
  net::URLRequestContextGetter* request_context =
      model_->GetProfile()->GetRequestContext();
  credential_button_.reset(new CredentialsItemView(
      this, &form_, password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL,
      CredentialsItemView::ACCOUNT_CHOOSER, request_context));
  credential_button_->set_owned_by_client();
  credential_button_->SetEnabled(false);
  Refresh();
}

ManageCredentialItemView::~ManageCredentialItemView() {
}

void ManageCredentialItemView::Refresh() {
  RemoveAllChildViews(true);
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  BuildColumnSet(layout);
  layout->StartRow(1, TWO_COLUMN_SET);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  if (form_deleted_) {
    delete_button_ = nullptr;
    views::Label* removed_account = new views::Label(
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED_ACCOUNT),
        rb->GetFontList(ui::ResourceBundle::SmallFont));
    removed_account->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(removed_account);
    undo_link_ =
        new views::Link(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO));
    undo_link_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    undo_link_->set_listener(this);
    undo_link_->SetUnderline(false);
    undo_link_->SetFontList(rb->GetFontList(ui::ResourceBundle::BoldFont));
    layout->AddView(undo_link_);
  } else {
    undo_link_ = nullptr;
    layout->AddView(credential_button_.get());
    delete_button_ = new views::ImageButton(this);
    delete_button_->SetImage(views::ImageButton::STATE_NORMAL,
                             rb->GetImageNamed(IDR_CLOSE_2).ToImageSkia());
    delete_button_->SetImage(views::ImageButton::STATE_HOVERED,
                             rb->GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
    delete_button_->SetImage(views::ImageButton::STATE_PRESSED,
                             rb->GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());
    layout->AddView(delete_button_);
  }

  GetLayoutManager()->Layout(this);
}

void ManageCredentialItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  DCHECK_EQ(delete_button_, sender);
  form_deleted_ = true;
  // TODO(vasilii): notify |model_| about the deletion.
  Refresh();
}

void ManageCredentialItemView::LinkClicked(views::Link* source,
                                           int event_flags) {
  DCHECK_EQ(undo_link_, source);
  form_deleted_ = false;
  // TODO(vasilii): notify |model_| about adding.
  Refresh();
}

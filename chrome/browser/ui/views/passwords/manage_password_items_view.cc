// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

enum ColumnSets {
  ONE_COLUMN_SET,
  TWO_COLUMN_SET,
  THREE_COLUMN_SET
};

void BuildColumnSetIfNeeded(views::GridLayout* layout, int column_set_id) {
  if (layout->GetColumnSet(column_set_id))
    return;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);

  // The username/"Deleted!"/Border field.
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        2,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  if (column_set_id >= TWO_COLUMN_SET) {
    // The password/"Undo!" field.
    column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::FILL,
                          1,
                          views::GridLayout::USE_PREF,
                          0,
                          0);
  }
  // If we're in manage-mode, we need another column for the delete button.
  if (column_set_id == THREE_COLUMN_SET) {
    column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
    column_set->AddColumn(views::GridLayout::TRAILING,
                          views::GridLayout::FILL,
                          0,
                          views::GridLayout::USE_PREF,
                          0,
                          0);
  }
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
}

void AddBorderRow(views::GridLayout* layout, SkColor color) {
  BuildColumnSetIfNeeded(layout, ONE_COLUMN_SET);
  layout->StartRowWithPadding(0, ONE_COLUMN_SET, 0,
                              views::kRelatedControlVerticalSpacing);
  views::Separator* border = new views::Separator(views::Separator::HORIZONTAL);
  border->SetColor(color);
  layout->AddView(border);
}

views::Label* GenerateUsernameLabel(const autofill::PasswordForm& form) {
  views::Label* label = new views::Label(
      form.username_value.empty()
          ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
          : form.username_value);
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

views::Label* GeneratePasswordLabel(const autofill::PasswordForm& form) {
  views::Label* label = new views::Label(form.password_value);
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetObscured(true);
  return label;
}

}  // namespace

// Manage credentials: stores credentials state and adds proper row to layout
// based on credential state.
class ManagePasswordItemsView::PasswordFormRow : public views::ButtonListener,
                                                 public views::LinkListener {
 public:
  PasswordFormRow(ManagePasswordItemsView* host,
                  const autofill::PasswordForm* password_form);
  ~PasswordFormRow() override = default;

  void AddRow(views::GridLayout* layout);

 private:
  void AddCredentialsRow(views::GridLayout* layout);
  void AddUndoRow(views::GridLayout* layout);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  void ResetControls();

  ManagePasswordItemsView* host_;
  const autofill::PasswordForm* password_form_;
  views::Link* undo_link_;
  views::ImageButton* delete_button_;
  bool deleted_;
};

ManagePasswordItemsView::PasswordFormRow::PasswordFormRow(
    ManagePasswordItemsView* host, const autofill::PasswordForm* password_form)
    : host_(host),
      password_form_(password_form),
      undo_link_(nullptr),
      delete_button_(nullptr),
      deleted_(false) {}

void ManagePasswordItemsView::PasswordFormRow::AddRow(
    views::GridLayout* layout) {
  if (deleted_) {
    AddUndoRow(layout);
  } else {
    AddCredentialsRow(layout);
  }
}

void ManagePasswordItemsView::PasswordFormRow::AddCredentialsRow(
    views::GridLayout* layout) {
  ResetControls();
  int column_set_id =
      host_->model_->state() == password_manager::ui::PENDING_PASSWORD_STATE
          ? TWO_COLUMN_SET
          : THREE_COLUMN_SET;
  BuildColumnSetIfNeeded(layout, column_set_id);
  layout->StartRowWithPadding(0, column_set_id, 0,
                              views::kRelatedControlVerticalSpacing);
  layout->AddView(GenerateUsernameLabel(*password_form_));
  layout->AddView(GeneratePasswordLabel(*password_form_));
  if (column_set_id == THREE_COLUMN_SET) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    delete_button_ = new views::ImageButton(this);
    delete_button_->SetImage(views::ImageButton::STATE_NORMAL,
                             rb->GetImageNamed(IDR_CLOSE_2).ToImageSkia());
    delete_button_->SetImage(views::ImageButton::STATE_HOVERED,
                             rb->GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
    delete_button_->SetImage(views::ImageButton::STATE_PRESSED,
                             rb->GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());
    layout->AddView(delete_button_);
  }
}

void ManagePasswordItemsView::PasswordFormRow::AddUndoRow(
    views::GridLayout* layout) {
  ResetControls();
  views::Label* text =
      new views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED));
  text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  undo_link_ =
      new views::Link(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO));
  undo_link_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  undo_link_->set_listener(this);
  undo_link_->SetUnderline(false);
  undo_link_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  BuildColumnSetIfNeeded(layout, TWO_COLUMN_SET);
  layout->StartRowWithPadding(0, TWO_COLUMN_SET, 0,
                              views::kRelatedControlVerticalSpacing);
  layout->AddView(text);
  layout->AddView(undo_link_);
}

void ManagePasswordItemsView::PasswordFormRow::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(delete_button_, sender);
  deleted_ = true;
  host_->NotifyPasswordFormStatusChanged(*password_form_, deleted_);
}

void ManagePasswordItemsView::PasswordFormRow::LinkClicked(views::Link* sender,
                                                           int event_flags) {
  DCHECK_EQ(undo_link_, sender);
  deleted_ = false;
  host_->NotifyPasswordFormStatusChanged(*password_form_, deleted_);
}

void ManagePasswordItemsView::PasswordFormRow::ResetControls() {
  delete_button_ = nullptr;
  undo_link_ = nullptr;
}

// ManagePasswordItemsView
ManagePasswordItemsView::ManagePasswordItemsView(
    ManagePasswordsBubbleModel* manage_passwords_bubble_model,
    const std::vector<const autofill::PasswordForm*>& password_forms)
    : model_(manage_passwords_bubble_model) {
  for (const autofill::PasswordForm* password_form : password_forms) {
    password_forms_rows_.push_back(
        ManagePasswordItemsView::PasswordFormRow(this, password_form));
  }
  AddRows();
}

ManagePasswordItemsView::~ManagePasswordItemsView() = default;

void ManagePasswordItemsView::AddRows() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  SkColor color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor);
  AddBorderRow(layout, color);
  for (auto& row : password_forms_rows_) {
    row.AddRow(layout);
    AddBorderRow(layout, color);
  }
  GetLayoutManager()->Layout(this);
}

void ManagePasswordItemsView::NotifyPasswordFormStatusChanged(
    const autofill::PasswordForm& password_form, bool deleted) {
  Refresh();
  // After the view is consistent, notify the model that the password needs to
  // be updated (either removed or put back into the store, as appropriate.
  model_->OnPasswordAction(password_form,
                           deleted
                               ? ManagePasswordsBubbleModel::REMOVE_PASSWORD
                               : ManagePasswordsBubbleModel::ADD_PASSWORD);
}

void ManagePasswordItemsView::Refresh() {
  DCHECK_NE(password_manager::ui::PENDING_PASSWORD_STATE, model_->state());
  RemoveAllChildViews(true);
  AddRows();
}

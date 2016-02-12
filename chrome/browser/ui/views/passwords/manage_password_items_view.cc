// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"

#include <numeric>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
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
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
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
}

scoped_ptr<views::Label> GenerateUsernameLabel(
    const autofill::PasswordForm& form) {
  scoped_ptr<views::Label> label(new views::Label(GetDisplayUsername(form)));
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

scoped_ptr<views::Label> GeneratePasswordLabel(
    const autofill::PasswordForm& form) {
  base::string16 text = form.federation_url.is_empty()
      ? form.password_value
      : l10n_util::GetStringFUTF16(
            IDS_PASSWORDS_VIA_FEDERATION,
            base::UTF8ToUTF16(form.federation_url.host()));
  scoped_ptr<views::Label> label(new views::Label(text));
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  if (form.federation_url.is_empty())
    label->SetObscured(true);
  return label;
}

scoped_ptr<views::ImageButton> GenerateDeleteButton(
    views::ButtonListener* listener) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<views::ImageButton> button(new views::ImageButton(listener));
  button->SetImage(views::ImageButton::STATE_NORMAL,
                   rb->GetImageNamed(IDR_CLOSE_2).ToImageSkia());
  button->SetImage(views::ImageButton::STATE_HOVERED,
                   rb->GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
  button->SetImage(views::ImageButton::STATE_PRESSED,
                   rb->GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());
  return button;
}

scoped_ptr<views::Label> GenerateDeletedPasswordLabel() {
  scoped_ptr<views::Label> text(new views::Label(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED)));
  text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  return text;
}

scoped_ptr<views::Link> GenerateUndoLink(views::LinkListener* listener) {
  scoped_ptr<views::Link> undo_link(new views::Link(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO)));
  undo_link->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  undo_link->set_listener(listener);
  undo_link->SetUnderline(false);
  undo_link->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  return undo_link;
}

}  // namespace

// Manage credentials: stores credentials state and adds proper row to layout
// based on credential state.
class ManagePasswordItemsView::PasswordFormRow : public views::ButtonListener,
                                                 public views::LinkListener {
 public:
  PasswordFormRow(ManagePasswordItemsView* host,
                  const autofill::PasswordForm* password_form,
                  int fixed_height);
  ~PasswordFormRow() override = default;

  void AddRow(views::GridLayout* layout);

  // Returns the fixed height for a row excluding padding. 0 means no fixed
  // height required.
  // In MANAGE_STATE a row may represent a credential or a deleted credential.
  // To avoid repositioning all the rows should have a fixed height.
  static int GetFixedHeight(password_manager::ui::State state);

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
  // The UI elements pointers are weak and owned by their parent.
  views::Link* undo_link_;
  views::ImageButton* delete_button_;
  const int fixed_height_;
  bool deleted_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormRow);
};

ManagePasswordItemsView::PasswordFormRow::PasswordFormRow(
    ManagePasswordItemsView* host,
    const autofill::PasswordForm* password_form,
    int fixed_height)
    : host_(host),
      password_form_(password_form),
      undo_link_(nullptr),
      delete_button_(nullptr),
      fixed_height_(fixed_height),
      deleted_(false) {}

void ManagePasswordItemsView::PasswordFormRow::AddRow(
    views::GridLayout* layout) {
  if (deleted_) {
    AddUndoRow(layout);
  } else {
    AddCredentialsRow(layout);
  }
}

int ManagePasswordItemsView::PasswordFormRow::GetFixedHeight(
    password_manager::ui::State state) {
  if (state != password_manager::ui::MANAGE_STATE)
    return 0;
  scoped_ptr<views::ImageButton> delete_button(GenerateDeleteButton(nullptr));
  scoped_ptr<views::Link> link(GenerateUndoLink(nullptr));
  scoped_ptr<views::Label> label(GenerateDeletedPasswordLabel());
  views::View* row_views[] = {delete_button.get(), link.get(), label.get()};
  return std::accumulate(row_views, row_views + arraysize(row_views), 0,
                         [](int max_height, const views::View* view) {
    return std::max(max_height, view->GetPreferredSize().height());
  });
}

void ManagePasswordItemsView::PasswordFormRow::AddCredentialsRow(
    views::GridLayout* layout) {
  ResetControls();
  int column_set_id =
      host_->model_->state() == password_manager::ui::MANAGE_STATE
          ? THREE_COLUMN_SET
          : TWO_COLUMN_SET;
  BuildColumnSetIfNeeded(layout, column_set_id);
  layout->StartRow(0, column_set_id);
  layout->AddView(GenerateUsernameLabel(*password_form_).release(), 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  0, fixed_height_);
  layout->AddView(GeneratePasswordLabel(*password_form_).release(), 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  0, fixed_height_);
  if (column_set_id == THREE_COLUMN_SET) {
    delete_button_ = GenerateDeleteButton(this).release();
    layout->AddView(delete_button_, 1, 1,
                    views::GridLayout::TRAILING, views::GridLayout::FILL,
                    0, fixed_height_);
  }
}

void ManagePasswordItemsView::PasswordFormRow::AddUndoRow(
    views::GridLayout* layout) {
  ResetControls();
  scoped_ptr<views::Label> text = GenerateDeletedPasswordLabel();
  scoped_ptr<views::Link> undo_link = GenerateUndoLink(this);
  undo_link_ = undo_link.get();
  BuildColumnSetIfNeeded(layout, TWO_COLUMN_SET);
  layout->StartRow(0, TWO_COLUMN_SET);
  layout->AddView(text.release(), 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  0, fixed_height_);
  layout->AddView(undo_link.release(), 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  0, fixed_height_);
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
  int fixed_height = PasswordFormRow::GetFixedHeight(model_->state());
  for (const autofill::PasswordForm* password_form : password_forms) {
    if (!password_form->is_public_suffix_match)
      password_forms_rows_.push_back(
          new PasswordFormRow(this, password_form, fixed_height));
  }
  AddRows();
}

ManagePasswordItemsView::~ManagePasswordItemsView() = default;

void ManagePasswordItemsView::AddRows() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  for (auto* row : password_forms_rows_) {
    if (row != password_forms_rows_[0])
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    row->AddRow(layout);
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
